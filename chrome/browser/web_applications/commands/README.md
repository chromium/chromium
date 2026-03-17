# Commands (`chrome/browser/web_applications/commands`)

This directory contains the implementations of the operations scheduleable by
the `WebAppCommandScheduler`.

Some command operations are just one synchronous operation. These can be
scheduled as a callback instead of requiring a full class implementation (see
`ScheduleCallback` or `ScheduleCallbackWithResult` in the scheduler).

## Role of Commands

Commands are the **exclusive mechanism** for altering the state of the web app
system and its interaction with the OS. They are designed to prevent race
conditions when multiple app installations, updates, or uninstalls are happening
concurrently.

By encapsulating logic within a `WebAppCommand`, the `WebAppCommandManager`
ensures that operations requiring exclusive access to resources (locks) are
sequenced correctly.

## Best Practices

- **Check Initial State:** Any number of queued operations can occur between
  scheduling a command and it being run. Thus, always re-check the initial state
  when the command starts, as (for example) the app may have just been
  uninstalled!.
- **Locking:** Locks are automatically acquired by the system before
  `StartWithLock` is called on the command. Ensure your command declares the
  correct lock granularity to minimize contention while keeping the system safe.
  Locks can sometimes be 'upgraded' via the `WebAppLockManager` object on the
  lock. It is OK to schedule a command from another command, but to prevent
  deadlocks, do not wait for it to complete before completing the original
  command.
- **Delegate to Jobs:** A command often acts as an orchestrator for smaller
  units of work called `jobs`. If a sequence of operations is reusable by three
  or more commands, implement it as a [job](../jobs/README.md).
- **Callback Guarantee:** The command's completion callback (passed to the base
  class on construction) is automatically called by the command infrastructure
  when `CompleteAndSelfDestruct` is called or if the system is shut down. This
  requires commands to **ensure** that `CompleteAndSelfDestruct()` is called after `StartWithLock()` is called,
  , otherwise the entire system can hang. Commands can assume the
  callback will be called after the command is destroyed, so no reentry is
  possible.
- **Debugging:** The command's `GetMutableDebugValue()` is visible in
  `chrome://web-app-internals`, making it extremely useful to populate with
  detailed progression or error states. `DVLOG`s exist in the
  `web_app_command_manager.cc` and `web_app_lock_manager.cc` which can be useful
  to print out command information when they are scheduled and completed (e.g.
  visible via the command line arg `--vmodule=web_app*=1`)
- **Metrics:** It is recommended that every command implements an UMA metric
  that records the outcome of the command. The easiest way to do this is to
  transform the callback that is passed to the base class constructor via a
  metrics recording lambda (e.g.
  `base::BindOnce([](Result result) { base::UmaHistogramEnumeration(...); return result;}).Then(std::move(callback))`).
  This guarantees the metrics are always recorded, even on shutdown.
