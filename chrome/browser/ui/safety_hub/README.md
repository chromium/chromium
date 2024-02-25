## Services

This directory contains the services related to Safety Hub. Each specific
service inherits from `SafetyHubService`, as defined in `safety_hub_service.h`.
The service will be periodically updated, depending on the frequency determined
by `GetRepeatedUpdateInterval()`. Additionally, an update will be launched every
time that the service is started (whenever the browser is started). Hence, it's
important to note that the update can be called more frequently than the
interval returned by `GetRepeatedUpdateInterval()`.

The update process consists of two stages, a background task and a UI thread
task.

The background task will be run on a separate thread in the background. The
function that needs to be run in the background should contain the functionality
of the update that is the most computation-heavy, to prevent blocking the UI
thread and possibly freezing the browser. The background task can return an
intermediate result, that will be passed along to the UI thread task. Ideally,
the background task should be a static function that will be returned by
`GetBackgroundTask()`. As this will be run in a thread other than the one the
service runs in, any arguments that are bound to the function should be
thread-safe. As the browser shuts down, references to these objects might be
destroyed, possibly leading to memory issues. For instance, a reference to the
service itself should NOT be bound to the function. This will result in crashes
of other tests, and could cause the browser to crash when one profile is shut
down.

The UI thread task needs to be defined in `UpdateOnUIThread()`. It will be
passed the intermediate result (a unique pointer to `SafetyHubService::Result`)
that was returned by the background task. This method will be run synchronously
on the UI thread after the background task has completed. The result by this UI
thread task will be the final result that the observers will be notified of.
`SafetyHubService::Result` will thus be used for 1) passing the intermediate
result of `GetBackgroundTask()` to `UpdateOnUIThread()`, and 2) the final result
that follows from running the update process of the service. To reduce
unnecessary overhead, it is suggested that the final result does not contain any
of the intermediate results, e.g. by creating a new `SafetyHubService::Result`
in `UpdateOnUIThread()`.

In order to make the latest result of the service always available just after
initialization of the service, the `InitializeLatestResult()` needs to be called
in the constructor of the derived services.  This function, which also needs to
be implemented by each service, has to set the `latest_result_` property.

Similarly, the `StartRepeatedUpdates()` function should also be called in the
constructor of each service. This method will start up the timer that
periodically run the update.

In summary, each Safety Hub service should implement the following functions:

 - `InitializeLatestResult()`: set the `latest_result_` property to the latest
   available result.
 - `GetRepeatedUpdateInterval()`: returns a `base::TimeDelta` that determines
   the minimal frequency of how often the service is updated.
 - `GetBackgroundTask()`: returns a bound (static) function that will be
   executed in the background, containing the computation-heavy part of the
   service's update.
 - `UpdateOnUIThread()`: will be run synchronously on the UI thread, and will
   further process the intermediate result of the background task.
 - `GetAsWeakRef()`: returns a weak pointer to the service, from the
   `WeakPtrFactory` of the derived service.

## Results

Each Safety Hub service has their own result type that inherits from
`SafetyHubService::Result`. This result should include the information that is
needed for displaying the information in the UI and being able to distinguish
two different results. To support serialization, the `ToDictValue()` method
needs to be implemented by the derived result classes. Furthermore, the derived
classes should have a constructor that takes a `base::Value::Dict` as argument
and restores the properties that are defined in the dictionary.

## Testing

As updating the service will run a task both in the background as well as on the
UI thread, it is advised to use the helper function
`UpdateSafetyHubServiceAsync(service)`, defined in `safety_hub_test_util.h` to
trigger an update of the service in tests.
