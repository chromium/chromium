// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_TYPES_H_
#define COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_TYPES_H_

#include <ostream>

// Types here are used to register KeepAlives.
// They Give indications about which kind of optimizations are allowed during
// the KeepAlive's lifetime. This allows to have more info about the state of
// the browser to optimize the resource consumption.

// Refers to the what the KeepAlive's lifetime is tied to, to help debugging.
enum class KeepAliveOrigin {
  // c/b
  APP_CONTROLLER,
  BROWSER,
  BROWSER_PROCESS_CHROMEOS,
  BROWSER_PROCESS_FUCHSIA,
  BROWSER_PROCESS_LACROS,
  SESSION_RESTORE,
  HEADLESS_COMMAND,

  // c/b/apps
  APP_LAUNCH,

  // c/b/background
  BACKGROUND_MODE_MANAGER,
  BACKGROUND_MODE_MANAGER_STARTUP,
  BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS,

  // c/b/background_sync
  BACKGROUND_SYNC,

  // c/b/browsing_data
  BROWSING_DATA_LIFETIME_MANAGER,

  // c/b/chromeos
  LOGIN_DISPLAY_HOST_WEBUI,
  PIN_MIGRATION,
  DRIVEFS_NATIVE_MESSAGE_HOST_LACROS,

  // c/b/devtools
  REMOTE_DEBUGGING,
  DEVTOOLS_WINDOW,

  // c/b/extensions
  NATIVE_MESSAGING_HOST_ERROR_REPORT,

  // c/b/notifications
  NOTIFICATION,
  PENDING_NOTIFICATION_CLICK_EVENT,
  PENDING_NOTIFICATION_CLOSE_EVENT,

  // c/b/push_messaging
  IN_FLIGHT_PUSH_MESSAGE,

  // c/b/ui
  APP_LIST_SERVICE_VIEWS,
  APP_LIST_SHOWER,
  CHROME_APP_DELEGATE,
  CHROME_VIEWS_DELEGATE,
  PANEL,
  PANEL_VIEW,
  PROFILE_MANAGER,
  USER_MANAGER_VIEW,
  CREDENTIAL_PROVIDER_SIGNIN_DIALOG,
  WEB_APP_INTENT_PICKER,

  // c/b/ui/web_applications
  WEB_APP_UNINSTALL,

  // c/b/web_applications
  APP_MANIFEST_UPDATE,
  APP_START_URL_MIGRATION,
  APP_GET_INFO,
  WEB_APP_LAUNCH,
  ISOLATED_WEB_APP_INSTALL,
  ISOLATED_WEB_APP_UPDATE,

  // c/b/sessions
  SESSION_DATA_DELETER,

  // components/metrics
  UMA_LOG,
};

// Restart: Allow Chrome to restart when all the registered KeepAlives allow
// restarts
enum class KeepAliveRestartOption { DISABLED, ENABLED };

std::ostream& operator<<(std::ostream& out, const KeepAliveOrigin& origin);
std::ostream& operator<<(std::ostream& out,
                         const KeepAliveRestartOption& restart);

#endif  // COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_TYPES_H_
