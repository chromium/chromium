# Isolated Web Apps Developer WebUI (`chrome/browser/ui/webui/iwa_dev`)

This directory contains the C++ WebUI controller and Mojo page handler for the
`chrome://iwa-dev` Isolated Web App (IWA) developer tools page.

## Companion Documentation

- **Frontend TypeScript/Lit Resources:**
  [`//chrome/browser/resources/iwa_dev`](/chrome/browser/resources/iwa_dev/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Key Components

- `IwaDevUI` (`iwa_dev_ui.{h,cc}`): `ui::MojoWebUIController` and
  `IwaDevUIConfig` registering the `chrome://iwa-dev` WebUI host and binding
  `iwa_dev::mojom::PageHandlerFactory` (with the page handler and UI gated on
  `AreIsolatedWebAppsEnabled` and `features::kIsolatedWebAppDevMode`).
- `IwaDevPageHandler` (`iwa_dev_page_handler.{h,cc}`): Implements
  `iwa_dev::mojom::PageHandler`, bridging WebUI actions to `WebAppProvider`:
  - Installing dev-mode IWAs from proxy URLs (`InstallAppFromDevProxy`), local
    `.swbn` bundles (`SelectAndInstallAppFromLocalWebBundle`), or Update
    Manifests (`ParseUpdateManifestFromUrl` and `InstallAppFromUpdateManifest`).
  - Inspecting (`GetInstalledAppsInfo`), launching (`LaunchApp`), updating
    (`UpdateDevProxyInstalledApp`, `SelectAndUpdateAppFromLocalWebBundle`,
    `UpdateManifestInstalledApp`, `SetUpdateChannel`), and uninstalling
    (`UninstallApp`) installed dev-mode IWAs
    (`WebAppFilter::IsDevModeIsolatedApp()`).
- `iwa_dev.mojom`: Mojo interface contract between the Lit frontend and browser
  process, reviewed by `ipc/SECURITY_OWNERS`.

## Testing

- **Browser Tests:** `iwa_dev_page_handler_browsertest.cc` verifies developer
  install, update, channel switching, and error handling end to end.
