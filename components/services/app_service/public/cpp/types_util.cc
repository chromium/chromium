// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/types_util.h"

#include "base/notreached.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace apps_util {

bool IsInstalled(apps::Readiness readiness) {
  switch (readiness) {
    case apps::Readiness::kReady:
    case apps::Readiness::kDisabledByBlocklist:
    case apps::Readiness::kDisabledByPolicy:
    case apps::Readiness::kDisabledByUser:
    case apps::Readiness::kTerminated:
    case apps::Readiness::kDisabledByLocalSettings:
      return true;
    case apps::Readiness::kUninstalledByUser:
    case apps::Readiness::kUninstalledByNonUser:
    case apps::Readiness::kRemoved:
    case apps::Readiness::kUnknown:
      return false;
  }
}

bool IsDisabled(apps::Readiness readiness) {
  switch (readiness) {
    case apps::Readiness::kDisabledByPolicy:
    case apps::Readiness::kDisabledByLocalSettings:
      return true;
    case apps::Readiness::kReady:
    case apps::Readiness::kDisabledByBlocklist:
    case apps::Readiness::kDisabledByUser:
    case apps::Readiness::kTerminated:
    case apps::Readiness::kUninstalledByUser:
    case apps::Readiness::kUninstalledByNonUser:
    case apps::Readiness::kRemoved:
    case apps::Readiness::kUnknown:
      return false;
  }
}

bool IsHumanLaunch(apps::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::LaunchSource::kFromAppListGrid:
    case apps::LaunchSource::kFromAppListGridContextMenu:
    case apps::LaunchSource::kFromAppListQuery:
    case apps::LaunchSource::kFromAppListQueryContextMenu:
    case apps::LaunchSource::kFromAppListRecommendation:
    case apps::LaunchSource::kFromParentalControls:
    case apps::LaunchSource::kFromShelf:
    case apps::LaunchSource::kFromFileManager:
    case apps::LaunchSource::kFromLink:
    case apps::LaunchSource::kFromOmnibox:
    case apps::LaunchSource::kFromKeyboard:
    case apps::LaunchSource::kFromOtherApp:
    case apps::LaunchSource::kFromMenu:
    case apps::LaunchSource::kFromInstalledNotification:
    case apps::LaunchSource::kFromSharesheet:
    case apps::LaunchSource::kFromReleaseNotesNotification:
    case apps::LaunchSource::kFromFullRestore:
    case apps::LaunchSource::kFromSmartTextContextMenu:
    case apps::LaunchSource::kFromDiscoverTabNotification:
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromLockScreen:
    case apps::LaunchSource::kFromAppHomePage:
    case apps::LaunchSource::kFromReparenting:
    case apps::LaunchSource::kFromProfileMenu:
    case apps::LaunchSource::kFromSysTrayCalendar:
    case apps::LaunchSource::kFromInstaller:
    case apps::LaunchSource::kFromWelcomeTour:
    case apps::LaunchSource::kFromFocusMode:
    case apps::LaunchSource::kFromNavigationCapturing:
      return true;
    case apps::LaunchSource::kUnknown:
    case apps::LaunchSource::kFromChromeInternal:
    case apps::LaunchSource::kFromTest:
    case apps::LaunchSource::kFromArc:
    case apps::LaunchSource::kFromManagementApi:
    case apps::LaunchSource::kFromKiosk:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromNewTabPage:
    case apps::LaunchSource::kFromIntentUrl:
    case apps::LaunchSource::kFromOsLogin:
    case apps::LaunchSource::kFromProtocolHandler:
    case apps::LaunchSource::kFromUrlHandler:
    case apps::LaunchSource::kFromFirstRun:
    case apps::LaunchSource::kFromSparky:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
}

bool AppTypeUsesWebContents(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
      return true;
    case apps::AppType::kUnknown:
    case apps::AppType::kArc:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
    case apps::AppType::kPluginVm:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kStandaloneBrowserExtension:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace apps_util
