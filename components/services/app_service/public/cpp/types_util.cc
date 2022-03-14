// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/types_util.h"

namespace apps_util {

bool IsInstalled(apps::mojom::Readiness readiness) {
  switch (readiness) {
    case apps::mojom::Readiness::kReady:
    case apps::mojom::Readiness::kDisabledByBlocklist:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kTerminated:
      return true;
    case apps::mojom::Readiness::kUninstalledByUser:
    case apps::mojom::Readiness::kUninstalledByMigration:
    case apps::mojom::Readiness::kRemoved:
    case apps::mojom::Readiness::kUnknown:
      return false;
  }
}

bool IsInstalled(apps::Readiness readiness) {
  switch (readiness) {
    case apps::Readiness::kReady:
    case apps::Readiness::kDisabledByBlocklist:
    case apps::Readiness::kDisabledByPolicy:
    case apps::Readiness::kDisabledByUser:
    case apps::Readiness::kTerminated:
      return true;
    case apps::Readiness::kUninstalledByUser:
    case apps::Readiness::kUninstalledByMigration:
    case apps::Readiness::kRemoved:
    case apps::Readiness::kUnknown:
      return false;
  }
}

bool IsHumanLaunch(apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
    case apps::mojom::LaunchSource::kFromParentalControls:
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
    case apps::mojom::LaunchSource::kFromCommandLine:
      return true;
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromManagementApi:
    case apps::mojom::LaunchSource::kFromKiosk:
    case apps::mojom::LaunchSource::kFromBackgroundMode:
    case apps::mojom::LaunchSource::kFromNewTabPage:
    case apps::mojom::LaunchSource::kFromIntentUrl:
    case apps::mojom::LaunchSource::kFromOsLogin:
    case apps::mojom::LaunchSource::kFromProtocolHandler:
    case apps::mojom::LaunchSource::kFromUrlHandler:
      return false;
  }
  NOTREACHED();
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
    case apps::AppType::kMacOs:
    case apps::AppType::kPluginVm:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kStandaloneBrowserExtension:
      return false;
  }
  NOTREACHED();
}

}  // namespace apps_util
