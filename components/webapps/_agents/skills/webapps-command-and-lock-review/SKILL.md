---
name: webapps-command-and-lock-review
description: >-
  Reviews changes in chrome/browser/web_applications/,
  chrome/browser/ui/web_applications/, and chrome/browser/ui/views/web_apps/ for
  WebAppCommand completion and locking invariants, unsafe WebAppProvider getter
  access, StartWithLock state re-validation, constructor-chained UMA metrics
  callbacks, and command scheduling deadlocks. Use when reviewing WebAppCommand,
  WebAppCommandScheduler, WebAppLockManager, or WebAppProvider changes.
---

### `WEBAPPS_COMMAND_MUST_COMPLETE_AND_SELF_DESTRUCT`: Every exit path after `StartWithLock()` must call `CompleteAndSelfDestruct()`

- **Severity**: ERROR
- **Why**: Once `WebAppCommand::StartWithLock()` runs, the command holds its
  `PartitionedLockManager` locks until `CompleteAndSelfDestruct()` is called. If
  any early return or continuation callback exits without calling
  `CompleteAndSelfDestruct()`, the lock is never released and the entire
  `WebAppCommandManager` queue hangs indefinitely.
- **Look for**:
  - `return;` or early-exit branches inside `StartWithLock()` or any `On*`
    continuation callback of a `WebAppCommand` that do not call
    `CompleteAndSelfDestruct()`.
  - Error branches that only log or only invoke a caller callback without
    calling `CompleteAndSelfDestruct()`.
  - `WeakPtr`-bound continuation callbacks where a failure or null-check path
    returns without completing the command.
- **Do not flag**: Paths that explicitly call `CompleteAndSelfDestruct()`
  (including via a helper method that always calls it), or code that executes
  before `StartWithLock()` is called.

### `WEBAPPS_STATE_MUTATION_REQUIRES_COMMAND`: Wrap Web App state mutations and multi-step async reads in a `WebAppCommand`

- **Severity**: ERROR
- **Why**: Modifying or acting on Web App state (registry, prefs, database, OS
  integration, or icons) outside a `WebAppCommand` bypasses
  `PartitionedLockManager` serialization and races with queued install, update,
  and uninstall commands.
- **Look for**: `ScopedRegistryUpdate` or `sync_bridge_unsafe()` writes, pref or
  database mutations keyed by `webapps::AppId`, or multi-step asynchronous flows
  that read `WebAppRegistrar` state and then mutate or act on it, located in
  managers, `KeyedService`s, UI controllers, or `TabHelper`s rather than inside
  a `WebAppCommand` or `Job`.
- **Fix**: Wrap the operation in a `WebAppCommand` scheduled via
  `provider->scheduler()`, or use a `CallbackCommand` via
  `scheduler().ScheduleCallback()` / `scheduler().ScheduleCallbackWithResult()`
  for synchronous lock-protected operations.
- **Do not flag**: Code inside a `WebAppCommand` or `Job` holding an `AppLock`
  or `AllAppsLock`, `CallbackCommand` usage via
  `scheduler().ScheduleCallback*()`, or `WebAppSyncBridge` internals.

### `WEBAPPS_NO_UNSAFE_PROVIDER_ACCESS_IN_ASYNC_OR_COMMANDS`: Do not call `registrar_unsafe()` or `sync_bridge_unsafe()` inside commands or across async callbacks

- **Severity**: ERROR
- **Why**: Subsystem state can mutate between asynchronous steps as queued
  commands execute. Calling `WebAppProvider::registrar_unsafe()` or
  `sync_bridge_unsafe()` inside a `WebAppCommand` or across asynchronous
  callbacks bypasses `PartitionedLockManager` guarantees and causes race
  conditions or stale-state reads.
- **Look for**:
  - Calls to `registrar_unsafe()` or `sync_bridge_unsafe()` inside any
    `WebAppCommand` (`chrome/browser/web_applications/commands/`) or Job
    (`chrome/browser/web_applications/jobs/`) instead of using the held lock's
    `WithAppResources` accessors (`lock_->registrar()` and
    `lock_->sync_bridge()`).
  - Calls to `registrar_unsafe()` or `sync_bridge_unsafe()` inside asynchronous
    callbacks (`base::BindOnce`, posted tasks) outside commands.
  - Newly added lock-free `registrar_unsafe()` reads outside commands whose
    result is then used to make a decision that mutates app state.
- **Fix**: Inside a command or job, use `lock_->registrar()` and
  `lock_->sync_bridge()`. Outside a command, wrap the flow in a `WebAppCommand`,
  or use a `CallbackCommand` via `scheduler().ScheduleCallbackWithResult()` for
  a short synchronous read under a lock.
- **Do not flag**: Accessing the registrar or sync bridge via a held lock
  (`lock_->registrar()`, `lock_->sync_bridge()`), synchronous read-only
  UI-thread `registrar_unsafe()` queries that do not drive app-state mutations,
  or test code (`*_unittest.cc`, `*_browsertest.cc`).

### `WEBAPPS_PREFER_LOCK_ACCESSORS_IN_COMMANDS`: Prefer `lock_->*()` resource accessors over `provider_->*()` inside commands and jobs

- **Severity**: WARNING
- **Why**: Lock classes inheriting from `WithAppResources` provide subsystem
  accessors (`registrar()`, `sync_bridge()`, `install_manager()`,
  `icon_manager()`, `os_integration_manager()`, `ui_manager()`,
  `extensions_manager()`, `translation_manager()`, `isolation_delegate()`) that
  `CHECK`-fail if accessed before the lock is granted or after `WebAppProvider`
  shutdown. Querying `provider_->*()` directly inside a command or job bypasses
  these lifetime and lock-grant checks.
- **Look for**: Calls to `provider_->icon_manager()`,
  `provider_->install_manager()`, `provider_->os_integration_manager()`,
  `provider_->ui_manager()`, `provider_->extensions_manager()`,
  `provider_->translation_manager()`, or `provider_->isolation_delegate()`
  inside a `WebAppCommand` or `Job` that holds a `WithAppResources` lock.
- **Do not flag**: Accessing subsystems via `lock_->*()`, or accessing
  `WebAppProvider` members that are not on `WithAppResources` (such as
  `clock()`, `file_utils()`, or `origin_association_manager()`).

### `WEBAPPS_RECHECK_STATE_IN_COMMAND_START_WITH_LOCK`: Re-validate initial state preconditions at the start of `WebAppCommand::StartWithLock()`

- **Severity**: ERROR
- **Why**: Commands are queued asynchronously in `WebAppCommandManager` while
  waiting to acquire their lock. Other queued commands may uninstall or modify
  the target app between the time the command was scheduled and when
  `StartWithLock()` runs.
- **Look for**: `StartWithLock()` implementations in `WebAppCommand` subclasses
  that operate on an existing `webapps::AppId` (for example, reading or mutating
  app state) without first verifying that initial preconditions still hold using
  `lock_->registrar().AppMatches(app_id_, WebAppFilter::...)` (preferring
  specialized `WebAppFilter` queries over catch-all `GetAppById()` checks) and
  calling `CompleteAndSelfDestruct()` early when invalid.
- **Do not flag**: `StartWithLock()` methods that validate app state with
  `AppMatches()` and a `WebAppFilter` and call `CompleteAndSelfDestruct()` early
  on mismatch, commands that install a new app where no prior app state is
  assumed, or `NoopLock` commands that do not read per-app registrar state.

### `WEBAPPS_CHAIN_UMA_METRICS_IN_COMMAND_CONSTRUCTOR`: Chain command result UMA metrics onto the completion callback in the `WebAppCommand` constructor

- **Severity**: WARNING
- **Why**: When `WebAppCommandManager::Shutdown()` aborts a queued or running
  command, the command manager invokes the base `WebAppCommand` completion
  callback with `args_for_shutdown` and destroys the command without reaching
  `CompleteAndSelfDestruct()` call sites. Logging the command's completion
  histogram only at `CompleteAndSelfDestruct()` call sites fails to record
  shutdown aborts in UMA.
- **Look for**:
  - `base::UmaHistogramEnumeration` or `base::UmaHistogramBoolean` calls that
    record a `WebAppCommand`'s overall result enum directly inside a
    `CompleteAndSelfDestruct()` helper instead of chaining a metrics-recording
    callback onto the completion callback in the `WebAppCommand` constructor via
    `.Then(std::move(callback))`.
  - Brand-new `WebAppCommand` subclasses that do not record a completion outcome
    histogram at all (report as `INFO`, as outcome metrics are recommended for
    every command).
- **Do not flag**: Chaining `base::BindOnce(...)` with
  `base::UmaHistogramEnumeration` and `.Then(std::move(callback))` in the
  `WebAppCommand` constructor initializer list, or recording intermediate
  step-specific histograms that are not the command's overall completion result.

### `WEBAPPS_NO_BLOCKING_WAIT_BETWEEN_COMMANDS`: Never schedule a child command from a running command and wait for its completion

- **Severity**: ERROR
- **Why**: A running `WebAppCommand` holds its `PartitionedLockManager` locks
  until `CompleteAndSelfDestruct()` is called. If a running command schedules a
  second command via `scheduler()` and waits for its completion callback before
  calling `CompleteAndSelfDestruct()`, the child command will wait forever in
  `PartitionedLockManager` if it requests an overlapping `AppLock` or
  `AllAppsLock`, causing an unrecoverable deadlock.
- **Look for**: Calls to `provider_->scheduler().*` or `scheduler_->*` inside a
  `WebAppCommand` subclass where the completion callback is bound back to `this`
  (e.g. `base::BindOnce(&MyCommand::OnSubCommandDone, ...)` via `GetWeakPtr()`)
  to continue execution before `CompleteAndSelfDestruct()`.
- **Do not flag**: Extracting shared logic into a reusable Job class in
  `chrome/browser/web_applications/jobs/` that runs under the parent command's
  existing `*lock_`, upgrading a lock via
  `lock_manager().UpgradeAndAcquireLock(...)`, or scheduling a follow-up command
  immediately before `CompleteAndSelfDestruct()` without awaiting its
  completion.
