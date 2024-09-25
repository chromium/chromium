// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_launch_util.h"

#include <ostream>

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
    case LaunchSource::kFromReparenting:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_REPARENTING;
    case LaunchSource::kFromProfileMenu:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_PROFILE_MENU;
    case LaunchSource::kFromSysTrayCalendar:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_SYSTEM_TRAY_CALENDAR;
    case LaunchSource::kFromInstaller:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_INSTALLER;
    case LaunchSource::kFromFirstRun:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_FIRST_RUN;
    case LaunchSource::kFromWelcomeTour:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_WELCOME_TOUR;
    case LaunchSource::kFromFocusMode:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_FOCUS_MODE;
    case LaunchSource::kFromSparky:
      return ApplicationLaunchSource::APPLICATION_LAUNCH_SOURCE_CHROME_INTERNAL;
    case LaunchSource::kFromNavigationCapturing:
      return ApplicationLaunchSource::
          APPLICATION_LAUNCH_SOURCE_NAVIGATION_CAPTURING;
  }
}

std::ostream& operator<<(std::ostream& out, LaunchSource launch_source) {
  switch (launch_source) {
    case LaunchSource::kUnknown:
      return out << "kUnknown";
    case LaunchSource::kFromAppListGrid:
      return out << "kFromAppListGrid";
    case LaunchSource::kFromAppListGridContextMenu:
      return out << "kFromAppListGridContextMenu";
    case LaunchSource::kFromAppListQuery:
      return out << "kFromAppListQuery";
    case LaunchSource::kFromAppListQueryContextMenu:
      return out << "kFromAppListQueryContextMenu";
    case LaunchSource::kFromAppListRecommendation:
      return out << "kFromAppListRecommendation";
    case LaunchSource::kFromParentalControls:
      return out << "kFromParentalControls";
    case LaunchSource::kFromShelf:
      return out << "kFromShelf";
    case LaunchSource::kFromFileManager:
      return out << "kFromFileManager";
    case LaunchSource::kFromLink:
      return out << "kFromLink";
    case LaunchSource::kFromOmnibox:
      return out << "kFromOmnibox";
    case LaunchSource::kFromChromeInternal:
      return out << "kFromChromeInternal";
    case LaunchSource::kFromKeyboard:
      return out << "kFromKeyboard";
    case LaunchSource::kFromOtherApp:
      return out << "kFromOtherApp";
    case LaunchSource::kFromMenu:
      return out << "kFromMenu";
    case LaunchSource::kFromInstalledNotification:
      return out << "kFromInstalledNotification";
    case LaunchSource::kFromTest:
      return out << "kFromTest";
    case LaunchSource::kFromArc:
      return out << "kFromArc";
    case LaunchSource::kFromSharesheet:
      return out << "kFromSharesheet";
    case LaunchSource::kFromReleaseNotesNotification:
      return out << "kFromReleaseNotesNotification";
    case LaunchSource::kFromFullRestore:
      return out << "kFromFullRestore";
    case LaunchSource::kFromSmartTextContextMenu:
      return out << "kFromSmartTextContextMenu";
    case LaunchSource::kFromDiscoverTabNotification:
      return out << "kFromDiscoverTabNotification";
    case LaunchSource::kFromManagementApi:
      return out << "kFromManagementApi";
    case LaunchSource::kFromKiosk:
      return out << "kFromKiosk";
    case LaunchSource::kFromCommandLine:
      return out << "kFromCommandLine";
    case LaunchSource::kFromBackgroundMode:
      return out << "kFromBackgroundMode";
    case LaunchSource::kFromNewTabPage:
      return out << "kFromNewTabPage";
    case LaunchSource::kFromIntentUrl:
      return out << "kFromIntentUrl";
    case LaunchSource::kFromOsLogin:
      return out << "kFromOsLogin";
    case LaunchSource::kFromProtocolHandler:
      return out << "kFromProtocolHandler";
    case LaunchSource::kFromUrlHandler:
      return out << "kFromUrlHandler";
    case LaunchSource::kFromLockScreen:
      return out << "kFromLockScreen";
    case LaunchSource::kFromAppHomePage:
      return out << "kFromAppHomePage";
    case LaunchSource::kFromReparenting:
      return out << "kFromReparenting";
    case LaunchSource::kFromProfileMenu:
      return out << "kFromProfileMenu";
    case LaunchSource::kFromSysTrayCalendar:
      return out << "kFromSysTrayCalendar";
    case LaunchSource::kFromInstaller:
      return out << "kFromInstaller";
    case LaunchSource::kFromFirstRun:
      return out << "kFromFirstRun";
    case LaunchSource::kFromWelcomeTour:
      return out << "kFromWelcomeTour";
    case LaunchSource::kFromFocusMode:
      return out << "kFromFocusMode";
    case LaunchSource::kFromSparky:
      return out << "kFromSparky";
    case LaunchSource::kFromNavigationCapturing:
      return out << "kFromNavigationCapturing";
  }
}

}  // namespace apps
