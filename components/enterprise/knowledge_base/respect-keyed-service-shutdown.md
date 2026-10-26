---
name: enterprise/respect-keyed-service-shutdown
rubric: safety
severity: should_fix
---

**Rule.** In `KeyedService` subclasses (such as `CloudProfileReportingService`)
that hold `base::ScopedObservation`s, timers, controllers, or sub-schedulers
referencing `Profile`, `CloudPolicyCore`, or other `KeyedService`s, override
`Shutdown()` to reset those members and invalidate weak pointers during
two-phase shutdown before the destructor runs.

**Why.** Waiting until the destructor rather than `KeyedService::Shutdown()` to
tear down schedulers, controllers, or observers that reference `Profile` or
other `KeyedService`s causes use-after-free and dangling pointer crashes during
profile teardown.

## Examples

Bad:

```cpp
CloudProfileReportingService::~CloudProfileReportingService() {
  // Too late: dependent KeyedServices and Profile state may already be
  // destroyed when the destructor runs.
  core_observation_.Reset();
  browser_launch_controller_.reset();
  ResetReportScheduler();
}
```

Good:

```cpp
void CloudProfileReportingService::Shutdown() {
  core_observation_.Reset();
  browser_launch_controller_.reset();
  ResetReportScheduler();
}
```
