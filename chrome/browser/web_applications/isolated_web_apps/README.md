# Isolated Web Apps Browser Integration (`chrome/browser/web_applications/isolated_web_apps`)

This directory contains the Chrome browser-process implementation of **Isolated
Web Apps (IWAs)** integrated with the per-profile `WebAppProvider` subsystem.

## Overview & Companion Documentation

- **AI Agent Rules & Invariants:** [AGENTS.md](AGENTS.md)
- **High-Level Design Overview:**
  [Isolated Web Apps Design Doc](/chrome/browser/web_applications/docs/isolated_web_apps.md)
- **Browser-Agnostic Core Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)
- **Views Installer UI:**
  [`//chrome/browser/ui/views/web_apps/isolated_web_apps`](/chrome/browser/ui/views/web_apps/isolated_web_apps/README.md)
- **Signed Web Bundles Foundation:**
  [`//components/web_package/signed_web_bundles`](/components/web_package/signed_web_bundles/README.md)

While
[`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)
provides browser-agnostic Signed Web Bundle parsing, reader caching, and domain
types (`IwaOrigin`, `IwaVersion`, `IwaSource`), this directory implements the
profile-scoped lifecycle operations that depend on `Profile`,
`WebAppCommandManager`, `WebAppRegistrar`, and OS/policy services.

## Key Responsibilities

- **Installation & Trust Verification:** Validates Signed Web Bundle integrity
  blocks against enterprise policies, key distribution allowlists, and developer
  mode settings before committing `IsolationData` to the `WebApp` database.
- **Two-Phase Atomic Updates:** Discovers updates via `UpdateManifest` JSON
  fetches, prepares and stores verified `.swbn` bundles alongside the active
  version (`IsolatedWebAppUpdatePrepareAndStoreCommand`), and applies updates
  atomically (`IsolatedWebAppApplyUpdateCommand`) once app windows close.
- **Enterprise Policy & ChromeOS Caching:** Processes
  `IsolatedWebAppInstallForceList` policy entries and maintains persistent
  Signed Web Bundle caches on ChromeOS for Managed Guest Sessions (MGS) and
  Kiosk sessions.
- **Origin & Storage Partition Isolation:** Maps each `IwaOrigin`
  (`isolated-app://<SignedWebBundleId>`) to an isolated non-default
  `content::StoragePartitionConfig` and enforces `Permissions-Policy` and
  Controlled Frame capabilities.

## Subdirectory Map

- [`commands/`](commands/README.md): Lock-guarded `WebAppCommand`
  implementations for IWA install, update prepare/apply, integrity checking,
  bundle cache management, and orphan cleanup.
- [`install/`](install/README.md): Developer-mode installation manager
  (`IsolatedWebAppDevInstallManager`) and non-policy install workflows.
- [`jobs/`](jobs/README.md): Composable sub-tasks (`PrepareInstallInfoJob`,
  `GetIsolatedWebAppSizeJob`) invoked by top-level `WebAppCommand`s.
- [`key_distribution/`](key_distribution/README.md): Browser-side preload and
  component-updater integration for the IWA Key Distribution component.
- [`policy/`](policy/README.md): `IsolatedWebAppPolicyManager`, `IwaInstaller`,
  and ChromeOS MGS/Kiosk bundle cache management (`IwaBundleCacheManager`,
  `IwaCacheClient`).
- [`update/`](update/README.md): `IsolatedWebAppUpdateManager`,
  `IsolatedWebAppUpdateCheckAndPrepareTask`, `IsolatedWebAppUpdateApplyTask`,
  and automatic/manual update scheduling.
- [`update_manifest/`](update_manifest/README.md): Fetching and JSON parsing for
  IWA Update Manifests (`UpdateManifest`, `UpdateManifestFetcher`).
- [`window_management/`](window_management/README.md): Window-open burst
  notification services, `window.close()` content setting observers, and
  focus/window management browser tests for IWAs.
- [`test/`](test/README.md): Hermetic test builders (`IsolatedWebAppBuilder`,
  `ManifestBuilder`, `BundledIsolatedWebApp`), fake update servers, and policy
  test utilities.

## Core Root Components

- **Embedder & Runtime (`:isolated_web_apps`):** `ChromeIwaClient`
  (`chrome_iwa_client.{h,cc}`), `ChromeContentBrowserClientIsolatedWebAppsPart`
  (`chrome_content_browser_client_isolated_web_apps_part.{h,cc}`),
  `InitializeIsolatedWebAppRuntime` (`runtime_init.{h,cc}`),
  `IsolatedWebAppThrottle` (`isolated_web_app_throttle.{h,cc}`), and
  `IwaPermissionsPolicyCache` (`iwa_permissions_policy_cache.{h,cc}`).
- **Trust, Metadata & User Install (`:web_applications`):**
  `IsolatedWebAppTrustChecker` (`isolated_web_app_trust_checker.{h,cc}`),
  `CheckTrustAndSignatures` (`trust_and_signature_verifier.{h,cc}`),
  `IsolatedWebAppUserInstalledManager`
  (`isolated_web_app_user_installed_manager.{h,cc}`), and
  `SignedWebBundleMetadata` (`signed_web_bundle_metadata.{h,cc}`).
- **Leaf Utilities (`:isolated_web_apps_no_pwa_core_deps`):**
  `IsolatedWebAppUrlInfo` (`isolated_web_app_url_info.{h,cc}`),
  `IsIwaDevModeEnabled` / `IsIwaUnmanagedInstallEnabled`
  (`isolated_web_app_features.{h,cc}`), `storage_util.{h,cc}`, and
  `remove_isolated_web_app_data.{h,cc}` (enforces
  `assert_no_deps = [ "//chrome/browser/web_applications" ]` to prevent circular
  dependencies).
