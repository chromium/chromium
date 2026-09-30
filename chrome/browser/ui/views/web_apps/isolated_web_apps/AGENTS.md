# Isolated Web Apps Views UI (`chrome/browser/ui/views/web_apps/isolated_web_apps`)

**Parent:**
[Desktop Web Apps Views UI](/chrome/browser/ui/views/web_apps/AGENTS.md) ·
[WebApps Guidelines](/components/webapps/AGENTS.md)

Views-based user installation wizard (`IsolatedWebAppInstallerCoordinator`,
`IsolatedWebAppInstallerViewController`, `IsolatedWebAppInstallerViewImpl`),
bundle installability verification UI, and multi-capture notification details
dialogs for Isolated Web Apps on Desktop and ChromeOS.

## Canonical Docs

- [IWA Views UI Architecture](README.md)
- [IWA Browser Engine Guidelines](/chrome/browser/web_applications/isolated_web_apps/AGENTS.md)

## UI & Architectural Invariants

- **Model-View-Controller Separation:** `IsolatedWebAppInstallerModel` holds
  dialog state (`Step`), `IsolatedWebAppInstallerViewController` drives async
  checks and commands (`InstallabilityChecker`, `WebAppCommandScheduler`), and
  `IsolatedWebAppInstallerViewImpl` renders pure Views layouts without touching
  `WebAppProvider` directly.
- **Coordinator Entry Point:** External callers outside this directory should
  launch or focus the installer dialog via `WebAppUiManager` (delegating to
  `LaunchIsolatedWebAppInstaller` in `isolated_web_app_installer_coordinator.h`)
  rather than instantiating the model, view, or controller directly.

## Command Line Execution

- **Unit & Browser Tests:**
  `tools/autotest.py -C out/Default chrome/browser/ui/views/web_apps/isolated_web_apps/`
- **Header & Visibility Check:**
  `gn check out/Default "//chrome/browser/ui/views/web_apps/isolated_web_apps/*"`
