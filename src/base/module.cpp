/*******************************************************************************
 * @file    module.cpp
 * @author  garou (xgaroux@gmail.com)
 * @brief   Base class for other modules with time and event functions.
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "module.hpp"

/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief Construct a new Module object
 */
Module::Module()
    : _delayTime(0)
    , _nextCallTime(0)
    , _suspended(false)
    , _availability(true)
#if defined(FREERTOS_USED)
    , xHandle(NULL)
#endif
{
}

#if defined(FREERTOS_USED)
/**
 * @brief FreeRTOS ONLY. Construct a new Module object with
 *      internal task initialization
 *
 * @param name human readable task name
 * @param stack task stack size
 * @param prior task priority
 */
Module::Module(const char* name, uint32_t stack, UBaseType_t prior)
    : _delayTime(0)
    , _nextCallTime(0)
    , _suspended(false)
    , _availability(true)
{
    xTaskCreate(task, name, stack, this, prior, &xHandle);
}

/**
 * @brief FreeRTOS ONLY. Internal task initialization function for later
 *      task start
 *
 * @param name human readable task name
 * @param stack task stack size
 * @param prior task priority
 */
void Module::taskInit(const char* name, uint32_t stack, UBaseType_t prior)
{
    if (xHandle == NULL)
        xTaskCreate(task, name, stack, this, prior, &xHandle);
}
#endif

/**
 * @brief Returns availability of the module to use. Is it configured correctly,
 *      has all drivers installed, etc. For example, in device-specific
 *      cases where some modules are not available on the current hardware,
 *      but the software still references those modules. Can be used inside or
 *      outside the module
 *
 * @return true if module can be used, otherwise false
 */
bool Module::isAvailable() const
{
    return _availability;
}

/**
 * @brief Returns the Time object to wait for the next call to this
 *      object's dispatcher
 *
 * @return const Time& new call delay time
 */
const Time& Module::delayTime() const
{
    return _delayTime;
}

/**
 * @brief Returns the Time object absolute time for the next call to this
 *      object's dispatcher.
 *
 * @return const Time& new call absolute time
 */
const Time& Module::nextCallTime() const
{
    return _nextCallTime;
}

/**
 * @brief Module dispatcher function. Calls the virtual dispatch function
 *      to actually work in the successor classes. Returns the Time object
 *      to wait for the next call to this object's dispatcher
 *
 * @return const Time new call delay time
 */
const Time Module::dispatcher()
{
    // If suspended - no processing, just return big delay time
    if (_suspended)
        return Time(24, 0, 0);

    // Dispatcher called only if was zero delay or time has come
    Time now = Time::now();
    if (_delayTime.isZero() || now >= _nextCallTime) {
        _delayTime = _dispatcher();    // Call actual dispatcher function
        _nextCallTime = Time::now() + _delayTime;
        return _delayTime;  // Full delay time just after dispatcher call
    } else {
        // Calculate estimated delay time when called early
        Time estimatedTime = now - _nextCallTime;
        return estimatedTime.toMsec() >= 0 ? estimatedTime : 0;
    }
}

/**
 * @brief Suspend dispatcher work until it will be resumed
 */
void Module::suspend()
{
#if defined(FREERTOS_USED)
    // Suspend by FreeRTOS function or set suspend flag for no RTOS work
    if (xHandle)
        vTaskSuspend(NULL);
    else
        _suspended = true;
#else
    // Set suspend flag for no RTOS work
    _suspended = true;
#endif
}

/**
 * @brief Causes the next dispatcher call to execute as soon as possible
 *      regardless of nextCallTime and delayTime
 */
void Module::resume()
{
    // Zeroing delayTime causes calling of the virtual _dispatcher function
    _delayTime = 0;
#if defined(FREERTOS_USED)
    // With FreeRTOS also notify or resume task if it in suspended state,
    // or reset suspend flag for no RTOS work
    if (xHandle) {
        if (eTaskGetState(xHandle) == eSuspended)
            vTaskResume(xHandle);
        else
            xTaskNotifyGive(xHandle);
    } else {
        _suspended = false;
    }
#else
    // Reset suspend flag for no RTOS work
    _suspended = false;
#endif
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Set the availability property of the module
 *
 * @param value new state
 */
void Module::setAvailability(bool value)
{
    _availability = value;
}

#if defined(FREERTOS_USED)
/**
 * @brief FreeRTOS ONLY. Task function for working with the dispatcher
 */
void Module::task(void *instance)
{
    // Task not needed without instance
    if (!instance)
        vTaskDelete(NULL);

    while (1) {
        Time delay = static_cast<Module*>(instance)->dispatcher();
        // Wait for delay time or break waiting on notification
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay.toMsec()));
    }
}
#endif

/***************************** END OF FILE ************************************/