# Isolated Web Apps Views UI (`chrome/browser/ui/views/web_apps/isolated_web_apps`)

This directory implements the Views-based desktop user interface for Isolated
Web Apps (IWAs), including the interactive `.swbn` installation wizard and
multi-capture notification details dialogs.

## Companion Documentation

- **AI Agent Rules & Invariants:** [AGENTS.md](AGENTS.md)
- **Desktop Views Guidelines:**
  [`//chrome/browser/ui/views/web_apps`](/chrome/browser/ui/views/web_apps/AGENTS.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Architecture & Key Components

The user-driven IWA installation flow follows a strict Model-View-Controller
(MVC) separation:

1. **Coordinator (`IsolatedWebAppInstallerCoordinator`):** Entry point invoked
   when the user launches an `.swbn` file. Owns the model, controller, and view
   lifecycle and invokes the completion callback when the wizard closes.
2. **Model (`IsolatedWebAppInstallerModel`):** Holds the current dialog `Step`
   (`kNone`, `kDisabled`, `kGetMetadata`, `kShowMetadata`, `kInstall`,
   `kInstallSuccess`) and active modal dialog descriptions without UI framework
   dependencies.
3. **Controller (`IsolatedWebAppInstallerViewController`):** Coordinates
   asynchronous operations with `WebAppCommandScheduler`,
   `InstallabilityChecker`
   (`isolated_web_app_user_installability_checker.{h,cc}`), and OS settings
   links, updating the model and notifying `IsolatedWebAppInstallerView`.
4. **View (`IsolatedWebAppInstallerViewImpl` & `IsolatedWebAppIdentityView`):**
   Builds the declarative `views::DialogDelegate` steps, progress bars, error
   alerts, and app metadata views.
5. **Supporting Views & Helpers:** `MultiCaptureNotificationDetailsView`
   (displays allowlisted IWAs with `getAllScreensMedia` permission),
   `UninstallSubAppIdentityView` (renders sub-app entries in uninstall prompts),
   and `CallbackDelayer` (enforces minimum progress display duration).

## Testing

- **Unit Tests:** `isolated_web_app_installer_view_controller_unittest.cc` and
  `callback_delayer_unittest.cc`.
- **Browser Tests:** `isolated_web_app_installer_browsertest.cc` and
  `isolated_web_app_installer_view_browsertest.cc`.
