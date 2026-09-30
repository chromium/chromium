# Isolated Web App Enterprise Policy & Bundle Cache (`chrome/browser/web_applications/isolated_web_apps/policy`)

This directory manages enterprise policy force-installation and ChromeOS bundle
caching for Isolated Web Apps (IWAs).

## Key Components

- `IsolatedWebAppPolicyManager` (`isolated_web_app_policy_manager.{h,cc}`):
  Observes the `IsolatedWebAppInstallForceList` pref and
  `IwaRuntimeDataProvider`, reconciles desired vs. installed policy IWAs under
  `AllAppsLock`, schedules uninstalls/updates, and queues `IwaInstaller` tasks.
- `IwaInstaller` (`isolated_web_app_installer.{h,cc}`): Executes the per-app
  policy installation flow for an `IsolatedWebAppExternalInstallOptions` entry
  (checking the ChromeOS bundle cache, fetching the `UpdateManifest`,
  downloading the `.swbn` bundle, scheduling `InstallIsolatedWebAppCommand`, and
  copying the installed bundle into the cache).
- `IwaBundleCacheManager` (`isolated_web_app_cache_manager.{h,cc}`) &
  `IwaCacheClient` (`isolated_web_app_cache_client.{h,cc}`): Manages persistent
  on-device `.swbn` caching on ChromeOS for Managed Guest Sessions (MGS) and
  Kiosk mode so ephemeral sessions can install IWAs without redundant network
  downloads.
