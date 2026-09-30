# Isolated Web App Window Management (`chrome/browser/web_applications/isolated_web_apps/window_management`)

This directory contains browser services and tests for window management
capabilities in Isolated Web Apps (IWAs).

## Key Components

- `IsolatedWebAppsWindowOpenPermissionService`, Factory, & Delegate
  (`isolated_web_apps_window_open_permission_service.{h,cc}`,
  `isolated_web_apps_window_open_permission_service_factory.{h,cc}`,
  `isolated_web_apps_window_open_permission_service_delegate.{h,cc}`): Tracks
  new windows/tabs opened by non-policy IWAs and displays a system notification
  (persisted via `IsolationData::OpenedTabsCounterNotificationState`) if an app
  opens a burst of windows in a short time window.
- `WindowManagementContentSettingObserver`
  (`window_management_content_setting_observer.{h,cc}`): Observes
  `ContentSettingsType::WINDOW_MANAGEMENT` changes and notifies `WebContents`
  (controlling script calls such as `window.close()`).
- Browser tests verifying `window.close()`, window focus, and multi-window burst
  notifications.
