# Isolated Web Apps Browser Engine (`chrome/browser/web_applications/isolated_web_apps`)

**Parent:**
[Desktop Web Applications](/chrome/browser/web_applications/AGENTS.md) ·
[WebApps Guidelines](/components/webapps/AGENTS.md)

Chrome browser-process implementation of Isolated Web Apps (IWAs) integrated
with `WebAppProvider`: installation, two-phase atomic updates, enterprise policy
force-installs, ChromeOS bundle caching (MGS/Kiosk), Controlled Frame
permissions, and `isolated-app://` navigation/URLLoader integration.

## Canonical Docs

- [IWA Browser Architecture & Subdirectory Map](README.md)
- [IWA High-Level Design Doc](/chrome/browser/web_applications/docs/isolated_web_apps.md)
- [Core IWA Component Guidelines](/components/webapps/isolated_web_apps/AGENTS.md)
- [IWA Views UI Guidelines](/chrome/browser/ui/views/web_apps/isolated_web_apps/AGENTS.md)
- [Commands Overview](commands/README.md)
- [Update Pipeline](update/README.md)
- [Enterprise Policy & Cache](policy/README.md)
- [WebApps Developer Skill](/components/webapps/_agents/skills/webapps-dev/SKILL.md)

## Architectural & Locking Invariants

- **Two-Phase Atomic Updates:** After `IsolatedWebAppUpdateCheckAndPrepareTask`
  downloads a candidate bundle, IWA updates are split into
  `IsolatedWebAppUpdatePrepareAndStoreCommand` (copies/moves the bundle into
  profile storage, verifies signatures, and dry-runs manifest extraction into
  `IsolationData::pending_update_info()`) and `IsolatedWebAppApplyUpdateCommand`
  (applies the pending update under `AppLock` only after all app windows close).
- **Command Observability (`GetMutableDebugValue`):** Every `WebAppCommand` in
  `commands/` MUST record input parameters, intermediate steps, and terminal
  results via `GetMutableDebugValue().Set(...)` so `chrome://web-app-internals`
  captures complete diagnostic traces.
- **Core Dependency Split (`assert_no_deps`):** Leaf targets in
  `:isolated_web_apps_no_pwa_core_deps` (`isolated_web_app_url_info`,
  `update_manifest`, `storage_util`, etc.) enforce
  `assert_no_deps = [ "//chrome/browser/web_applications" ]` to prevent circular
  dependencies.
- **`registrar_unsafe()` Discipline:** Synchronous calls to
  `provider_->registrar_unsafe()` outside a command lock are permitted ONLY on
  the UI thread for read-only queries and should document why lock-free access
  is safe.
- **Storage Partition Isolation:** Each IWA runs in a dedicated non-default
  `content::StoragePartitionConfig` derived from its `IwaOrigin`. Never mix IWA
  storage partitions with default browser partitions or across distinct bundle
  IDs.
- **Strong Types & Error Propagation:** Use `IwaOrigin`, `SignedWebBundleId`,
  `IwaVersion`, `IwaSourceWithModeAndFileOp`, and `base::expected<T, E>` with
  `ASSIGN_OR_RETURN` / `RETURN_IF_ERROR` across all install, update, and policy
  flows.

## Testing Guardrails

- **Hermetic Bundle Generation:** Build test `.swbn` bundles in scoped temp dirs
  (or launch a dev-mode proxy server) with `IsolatedWebAppBuilder`,
  `ManifestBuilder`, and `BundledIsolatedWebApp`
  (`test/isolated_web_app_builder.h`).
- **Fake Subsystems First:** Prefer `FakeWebAppProvider`,
  `FakeWebContentsManager`, and `base::test::TestFuture` in unit tests over
  heavyweight browser tests whenever testing command, policy, or update state
  machines.

## Command Line Execution

- **IWA Browser Unit Tests:**
  `tools/autotest.py -C out/Default chrome/browser/web_applications/isolated_web_apps/`
- **Header & Visibility Check:**
  `gn check out/Default "//chrome/browser/web_applications/isolated_web_apps/*"`
