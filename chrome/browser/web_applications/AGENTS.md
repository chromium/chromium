# Desktop Web Applications (`chrome/browser/web_applications`)

**Parent:** [WebApps Central Hub](/components/webapps/AGENTS.md)

Core desktop Progressive Web App (PWA) backend centered around the per-profile
`WebAppProvider` system: async command execution, fine-grained locking, LevelDB
storage, sync, and OS integration.

## Canonical Docs

- [Commands Architecture](commands/README.md)
- [Lock System](locks/README.md)
- [Jobs](jobs/README.md)
- [Desktop Testing Guide](docs/testing.md)

## Command Scheduling & Locking Rules

1. **State modification requires a Command:** Operations modifying or reading
   state (prefs, DB, disk) must use a `WebAppCommand` (or `CallbackCommand` if
   synchronous).
2. **Re-check State on Start:** Commands start asynchronously and wait on locks.
   Always re-validate initial state in `StartWithLock()` (e.g. app uninstalled
   while queued).
3. **`WebAppProvider::*_unsafe()` Access:** Permitted ONLY for synchronous
   read-only UI-thread queries that are proven safe (requires explanatory
   comment). Never use across async gaps (`await`, callbacks, posted tasks).
4. **Metrics Callback Chaining in Constructor:** Guarantee outcome metrics on
   shutdown/destruction by chaining the completion callback in constructor
   initialization:
   ```cpp
   base::BindOnce([](Result result) {
     base::UmaHistogramEnumeration("WebApp.Command.Result", result);
     return result;
   }).Then(std::move(callback))
   ```
5. **`CompleteAndSelfDestruct()` Contract:** Once `StartWithLock()` is called,
   the command MUST call `CompleteAndSelfDestruct()` to release locks and
   prevent system hangs.
6. **Lock Upgrades & Deadlock Prevention:** Hold locks for the command lifetime
   or upgrade via `WebAppLockManager`. Never release and re-acquire locks, and
   never wait synchronously on another command.
7. **Access System via Lock Getters:** Access subsystems via lock resource
   getters (e.g. `WithAppResources`) rather than querying `WebAppProvider`
   directly.

## Testing Guardrails

- **Do Not Mock WebAppProvider:** Never mock internal managers. Use
  `FakeWebAppProvider` and its standard fakes (`FakeWebContentsManager`,
  `FakeWebAppUiManager`, `FakeOsIntegrationManager`).
- **Fake Startup Timing:** Configure dependency fakes *before*
  `test::AwaitStartWebAppProviderAndSubsystems(profile())`.
- **Command Synchronization:** Flush asynchronous commands via
  `provider().command_manager().AwaitAllCommandsCompleteForTesting()`.
- **Navigation & Manifests (`WebAppPageWaiter`):** Prefer `WebAppPageWaiter`
  with `waiter.WaitAndFlushCommands()` in browser tests rather than arbitrary
  sleeps or timer loops.
- **OS Integration Assertion:** Use `OsIntegrationTestOverride`
  (`fake_os_integration()` or `os_integration_override()`) to assert OS state
  (e.g. shortcuts) hermetically.

## Command Line Execution

- **Desktop Unit Tests:**
  `tools/autotest.py -C out/Default chrome/browser/web_applications/web_app_unittest.cc`
- **Browser Tests:**
  `tools/autotest.py -C out/Default chrome/browser/ui/views/web_apps/web_app_integration_browsertest.cc`
