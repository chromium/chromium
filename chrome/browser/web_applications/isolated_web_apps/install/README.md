# Isolated Web App Installation (`chrome/browser/web_applications/isolated_web_apps/install`)

This directory implements installation entry points and developer-mode workflows
for Isolated Web Apps (IWAs).

## Key Components

- `IsolatedWebAppInstallSource` (`isolated_web_app_install_source.{h,cc}`):
  Pairs an `IwaSourceWithModeAndFileOp` with a `webapps::WebappInstallSource`
  via typed factory methods (`FromGraphicalInstaller`, `FromExternalPolicy`,
  `FromKiosk`, `FromShimlessRma`, `FromDevUi`, `FromDevCommandLine`).
- `IsolatedWebAppDevInstallManager`
  (`isolated_web_app_dev_install_manager.{h,cc}`): Handles installing IWAs in
  developer mode (from proxy URLs, local `.swbn` files, or remote `.swbn` URLs
  triggered via `chrome://web-app-internals`, the `PWA.install` DevTools
  protocol handler, or CLI switches).
- `NonInstalledBundleInspectionContext` & `IwaOperation`
  (`non_installed_bundle_inspection_context.{h,cc}`): `WebContentsUserData`
  attached to a `WebContents` during install, update preparation/application, or
  pre-install metadata reading so `ChromeIwaClient`,
  `IsolatedWebAppTrustChecker`, and `IsolatedWebAppThrottle` can serve and
  validate resources from a non-installed bundle or proxy source.

See [../README.md](../README.md) and
[../commands/README.md](../commands/README.md) for the underlying
`InstallIsolatedWebAppCommand`.
