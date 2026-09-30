# Isolated Web App Commands (`chrome/browser/web_applications/isolated_web_apps/commands`)

This directory contains `WebAppCommand` implementations for Isolated Web Apps
(IWAs), scheduled via `WebAppCommandScheduler` and synchronized through
`WebAppLockManager`.

## Key Commands

- **Installation & Updates:** `InstallIsolatedWebAppCommand`,
  `IsolatedWebAppUpdatePrepareAndStoreCommand`
  (`isolated_web_app_prepare_and_store_update_command.{h,cc}`), and
  `IsolatedWebAppApplyUpdateCommand`.
- **Verification, Partitions & Browsing Data:**
  `CheckIsolatedWebAppBundleUserInstallabilityCommand`,
  `GetControlledFramePartitionWithLock`
  (`get_controlled_frame_partition_command.{h,cc}`), and
  `GetIsolatedWebAppBrowsingDataCommand`.
- **Cache & Storage Hygiene:** `CleanupOrphanedIsolatedWebAppsCommand`, plus
  ChromeOS bundle cache commands (`CopyBundleToCacheCommand`,
  `GetBundleCachePathCommand`, `CleanupBundleCacheCommand`, and
  `RemoveObsoleteBundleVersionsCacheCommand`).

Commands record structured diagnostic state via `GetMutableDebugValue()` for
inspection in `chrome://web-app-internals`.
