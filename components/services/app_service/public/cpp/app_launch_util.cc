// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace apps {

WindowInfo::WindowInfo(int64_t display_id) : display_id(display_id) {}

LaunchSource ConvertMojomLaunchSourceToLaunchSource(
    apps::mojom::LaunchSource mojom_install_source) {
  switch (mojom_install_source) {
    case apps::mojom::LaunchSource::kUnknown:
      return LaunchSource::kUnknown;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      return LaunchSource::kFromAppListGrid;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      return LaunchSource::kFromAppListGridContextMenu;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      return LaunchSource::kFromAppListQuery;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      return LaunchSource::kFromAppListQueryContextMenu;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      return LaunchSource::kFromAppListRecommendation;
    case apps::mojom::LaunchSource::kFromParentalControls:
      return LaunchSource::kFromParentalControls;
    case apps::mojom::LaunchSource::kFromShelf:
      return LaunchSource::kFromShelf;
    case apps::mojom::LaunchSource::kFromFileManager:
      return LaunchSource::kFromFileManager;
    case apps::mojom::LaunchSource::kFromLink:
      return LaunchSource::kFromLink;
    case apps::mojom::LaunchSource::kFromOmnibox:
      return LaunchSource::kFromOmnibox;
    case apps::mojom::LaunchSource::kFromChromeInternal:
      return LaunchSource::kFromChromeInternal;
    case apps::mojom::LaunchSource::kFromKeyboard:
      return LaunchSource::kFromKeyboard;
    case apps::mojom::LaunchSource::kFromOtherApp:
      return LaunchSource::kFromOtherApp;
    case apps::mojom::LaunchSource::kFromMenu:
      return LaunchSource::kFromMenu;
    case apps::mojom::LaunchSource::kFromInstalledNotification:
      return LaunchSource::kFromInstalledNotification;
    case apps::mojom::LaunchSource::kFromTest:
      return LaunchSource::kFromTest;
    case apps::mojom::LaunchSource::kFromArc:
      return LaunchSource::kFromArc;
    case apps::mojom::LaunchSource::kFromSharesheet:
      return LaunchSource::kFromSharesheet;
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
      return LaunchSource::kFromReleaseNotesNotification;
    case apps::mojom::LaunchSource::kFromFullRestore:
      return LaunchSource::kFromFullRestore;
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      return LaunchSource::kFromSmartTextContextMenu;
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
      return LaunchSource::kFromDiscoverTabNotification;
    case apps::mojom::LaunchSource::kFromManagementApi:
      return LaunchSource::kFromManagementApi;
    case apps::mojom::LaunchSource::kFromKiosk:
      return LaunchSource::kFromKiosk;
    case apps::mojom::LaunchSource::kFromCommandLine:
      return LaunchSource::kFromCommandLine;
    case apps::mojom::LaunchSource::kFromBackgroundMode:
      return LaunchSource::kFromBackgroundMode;
    case apps::mojom::LaunchSource::kFromNewTabPage:
      return LaunchSource::kFromNewTabPage;
    case apps::mojom::LaunchSource::kFromIntentUrl:
      return LaunchSource::kFromIntentUrl;
    case apps::mojom::LaunchSource::kFromOsLogin:
      return LaunchSource::kFromOsLogin;
    case apps::mojom::LaunchSource::kFromProtocolHandler:
      return LaunchSource::kFromProtocolHandler;
    case apps::mojom::LaunchSource::kFromUrlHandler:
      return LaunchSource::kFromUrlHandler;
  }
}

}  // namespace apps
