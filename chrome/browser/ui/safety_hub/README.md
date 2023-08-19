This directory contains the services related to Safety Hub. Each specific
service inherits from `SafetyHubService`, as defined in `safety_hub_service.h`.
The services override the `UpdateOnBackgroundThread()` and
`GetRepeatedUpdateInterval()` methods. The former contains the functionality
that will be periodically executed, whereas the latter determines the interval
in which the function is executed.