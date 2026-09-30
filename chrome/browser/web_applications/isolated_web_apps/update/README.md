# Isolated Web App Update Pipeline (`chrome/browser/web_applications/isolated_web_apps/update`)

This directory implements automatic and manual update discovery, preparation,
and atomic application for Isolated Web Apps (IWAs).

## Key Components

- `IsolatedWebAppUpdateManager` (`isolated_web_app_update_manager.{h,cc}`):
  Per-profile manager that schedules periodic update check/prepare tasks
  (`IsolatedWebAppUpdateCheckAndPrepareTask`), reacts to key rotation events,
  and coordinates `IsolatedWebAppUpdateApplyTask` execution via
  `IsolatedWebAppUpdateApplyWaiter`.
- `IsolatedWebAppUpdateCheckAndPrepareTask`
  (`isolated_web_app_update_check_and_prepare_task.{h,cc}`): Fetches the app's
  `UpdateManifest`, selects the target version for the configured
  `UpdateChannel` (and optional pinned version), validates version changes via
  `ValidateVersionChangeFeasibility` (`version_change_validator.{h,cc}`),
  downloads the `.swbn` bundle, and invokes
  `IsolatedWebAppUpdatePrepareAndStoreCommand`.
- `IsolatedWebAppUpdateApplyWaiter`
  (`isolated_web_app_update_apply_waiter.{h,cc}`): Holds keep-alive objects and
  waits for all open windows of an IWA to close before signaling that a pending
  update can be applied.
- `IsolatedWebAppUpdateApplyTask` (`isolated_web_app_update_apply_task.{h,cc}`):
  Runs `IsolatedWebAppApplyUpdateCommand` to promote
  `IsolationData::pending_update_info()` to the active version (and updates the
  ChromeOS bundle cache when enabled).
- `IsolatedWebAppUpdateNotificationService`
  (`isolated_web_app_update_notification_service.{h,cc}`, ChromeOS): Displays a
  system notification when an update is staged for an open IWA so the user can
  restart the app immediately.
