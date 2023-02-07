// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace apps {

WindowInfo::WindowInfo(int64_t display_id) : display_id(display_id) {}

ApplicationLaunchSource ConvertLaunchSourceToProtoApplicationLaunchSource(
    LaunchSource launch_source) {
  switch (launch_source) {
    case LaunchSource::kUnknown:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_UNKNOWN;
    case LaunchSource::kFromAppListGrid:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_APP_LIST_GRID;
    case LaunchSource::kFromAppListGridContextMenu:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_APP_LIST_GRID_CONTEXT_MENU;
    case LaunchSource::kFromAppListQuery:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_APP_LIST_QUERY;
    case LaunchSource::kFromAppListQueryContextMenu:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_APP_LIST_QUERY_CONTEXT_MENU;
    case LaunchSource::kFromAppListRecommendation:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_APP_LIST_RECOMMENDATION;
    case LaunchSource::kFromParentalControls:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_PARENTAL_CONTROLS;
    case LaunchSource::kFromShelf:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_SHELF;
    case LaunchSource::kFromFileManager:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_FILE_MANAGER;
    case LaunchSource::kFromLink:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_LINK;
    case LaunchSource::kFromOmnibox:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_OMNIBOX;
    case LaunchSource::kFromChromeInternal:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_CHROME_INTERNAL;
    case LaunchSource::kFromKeyboard:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_KEYBOARD;
    case LaunchSource::kFromOtherApp:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_OTHER_APP;
    case LaunchSource::kFromMenu:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_MENU;
    case LaunchSource::kFromInstalledNotification:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_INSTALLED_NOTIFICATION;
    case LaunchSource::kFromTest:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_TEST;
    case LaunchSource::kFromArc:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_ARC;
    case LaunchSource::kFromSharesheet:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_SHARESHEET;
    case LaunchSource::kFromReleaseNotesNotification:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_RELEASE_NOTES_NOTIFICATION;
    case LaunchSource::kFromFullRestore:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_FULL_RESTORE;
    case LaunchSource::kFromSmartTextContextMenu:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_SMART_TEXT_CONTEXT_MENU;
    case LaunchSource::kFromDiscoverTabNotification:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_DISCOVER_TAB_NOTIFICATION;
    case LaunchSource::kFromManagementApi:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_MANAGEMENT_API;
    case LaunchSource::kFromKiosk:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_KIOSK;
    case LaunchSource::kFromCommandLine:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_COMMAND_LINE;
    case LaunchSource::kFromBackgroundMode:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_BACKGROUND_MODE;
    case LaunchSource::kFromNewTabPage:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_NEW_TAB_PAGE;
    case LaunchSource::kFromIntentUrl:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_INTENT_URL;
    case LaunchSource::kFromOsLogin:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_OS_LOGIN;
    case LaunchSource::kFromProtocolHandler:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_PROTOCOL_HANDLER;
    case LaunchSource::kFromUrlHandler:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_URL_HANDLER;
    case LaunchSource::kFromLockScreen:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_LOCK_SCREEN;
    case LaunchSource::kFromAppHomePage:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_APP_HOME_PAGE;
  }
}

}  // namespace apps
