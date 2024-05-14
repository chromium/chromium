// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keep_alive_registry/keep_alive_types.h"
#include "base/logging.h"
#include "base/notreached.h"

std::ostream& operator<<(std::ostream& out, const KeepAliveOrigin& origin) {
  switch (origin) {
    case KeepAliveOrigin::APP_CONTROLLER:
      return out << "APP_CONTROLLER";
    case KeepAliveOrigin::BROWSER:
      return out << "BROWSER";
    case KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS:
      return out << "BROWSER_PROCESS_CHROMEOS";
    case KeepAliveOrigin::BROWSER_PROCESS_FUCHSIA:
      return out << "BROWSER_PROCESS_FUCHSIA";
    case KeepAliveOrigin::BROWSER_PROCESS_LACROS:
      return out << "BROWSER_PROCESS_LACROS";
    case KeepAliveOrigin::SESSION_RESTORE:
      return out << "SESSION_RESTORE";
    case KeepAliveOrigin::HEADLESS_COMMAND:
      return out << "HEADLESS_COMMAND";
    case KeepAliveOrigin::APP_LAUNCH:
      return out << "APP_LAUNCH";
    case KeepAliveOrigin::BACKGROUND_MODE_MANAGER:
      return out << "BACKGROUND_MODE_MANAGER";
    case KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP:
      return out << "BACKGROUND_MODE_MANAGER_STARTUP";
    case KeepAliveOrigin::BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS:
      return out << "BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS";
    case KeepAliveOrigin::BACKGROUND_SYNC:
      return out << "BACKGROUND_SYNC";
    case KeepAliveOrigin::BROWSING_DATA_LIFETIME_MANAGER:
      return out << "BROWSING_DATA_LIFETIME_MANAGER";
    case KeepAliveOrigin::LOGIN_DISPLAY_HOST_WEBUI:
      return out << "LOGIN_DISPLAY_HOST_WEBUI";
    case KeepAliveOrigin::PIN_MIGRATION:
      return out << "PIN_MIGRATION";
    case KeepAliveOrigin::DRIVEFS_NATIVE_MESSAGE_HOST_LACROS:
      return out << "DRIVEFS_NATIVE_MESSAGE_HOST_LACROS";
    case KeepAliveOrigin::REMOTE_DEBUGGING:
      return out << "REMOTE_DEBUGGING";
    case KeepAliveOrigin::DEVTOOLS_WINDOW:
      return out << "DEVTOOLS_WINDOW";
    case KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT:
      return out << "NATIVE_MESSAGING_HOST_ERROR_REPORT";
    case KeepAliveOrigin::NOTIFICATION:
      return out << "NOTIFICATION";
    case KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT:
      return out << "PENDING_NOTIFICATION_CLICK_EVENT";
    case KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT:
      return out << "PENDING_NOTIFICATION_CLOSE_EVENT";
    case KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE:
      return out << "IN_FLIGHT_PUSH_MESSAGE";
    case KeepAliveOrigin::APP_LIST_SERVICE_VIEWS:
      return out << "APP_LIST_SERVICE_VIEWS";
    case KeepAliveOrigin::APP_LIST_SHOWER:
      return out << "APP_LIST_SHOWER";
    case KeepAliveOrigin::CHROME_APP_DELEGATE:
      return out << "CHROME_APP_DELEGATE";
    case KeepAliveOrigin::CHROME_VIEWS_DELEGATE:
      return out << "CHROME_VIEWS_DELEGATE";
    case KeepAliveOrigin::PANEL:
      return out << "PANEL";
    case KeepAliveOrigin::PANEL_VIEW:
      return out << "PANEL_VIEW";
    case KeepAliveOrigin::PROFILE_MANAGER:
      return out << "PROFILE_MANAGER";
    case KeepAliveOrigin::USER_MANAGER_VIEW:
      return out << "USER_MANAGER_VIEW";
    case KeepAliveOrigin::CREDENTIAL_PROVIDER_SIGNIN_DIALOG:
      return out << "CREDENTIAL_PROVIDER_SIGNIN_DIALOG";
    case KeepAliveOrigin::WEB_APP_INTENT_PICKER:
      return out << "WEB_APP_INTENT_PICKER";
    case KeepAliveOrigin::WEB_APP_UNINSTALL:
      return out << "WEB_APP_UNINSTALL";
    case KeepAliveOrigin::APP_MANIFEST_UPDATE:
      return out << "APP_MANIFEST_UPDATE";
    case KeepAliveOrigin::APP_START_URL_MIGRATION:
      return out << "APP_START_URL_MIGRATION";
    case KeepAliveOrigin::APP_GET_INFO:
      return out << "APP_GET_INFO";
    case KeepAliveOrigin::WEB_APP_LAUNCH:
      return out << "WEB_APP_LAUNCH";
    case KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL:
      return out << "ISOLATED_WEB_APP_INSTALL";
    case KeepAliveOrigin::ISOLATED_WEB_APP_UPDATE:
      return out << "ISOLATED_WEB_APP_UPDATE";
    case KeepAliveOrigin::SESSION_DATA_DELETER:
      return out << "SESSION_DATA_DELETER";
    case KeepAliveOrigin::UMA_LOG:
      return out << "UMA_LOG";
  }

  NOTREACHED_IN_MIGRATION();
  return out << static_cast<int>(origin);
}

std::ostream& operator<<(std::ostream& out,
                         const KeepAliveRestartOption& restart) {
  switch (restart) {
    case KeepAliveRestartOption::DISABLED:
      return out << "DISABLED";
    case KeepAliveRestartOption::ENABLED:
      return out << "ENABLED";
  }

  NOTREACHED_IN_MIGRATION();
  return out << static_cast<int>(restart);
}
