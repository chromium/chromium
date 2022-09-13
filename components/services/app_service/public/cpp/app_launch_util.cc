// Copyright 2022 The Chromium Authors
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

apps::mojom::LaunchSource ConvertLaunchSourceToMojomLaunchSource(
    LaunchSource install_source) {
  switch (install_source) {
    case LaunchSource::kUnknown:
      return apps::mojom::LaunchSource::kUnknown;
    case LaunchSource::kFromAppListGrid:
      return apps::mojom::LaunchSource::kFromAppListGrid;
    case LaunchSource::kFromAppListGridContextMenu:
      return apps::mojom::LaunchSource::kFromAppListGridContextMenu;
    case LaunchSource::kFromAppListQuery:
      return apps::mojom::LaunchSource::kFromAppListQuery;
    case LaunchSource::kFromAppListQueryContextMenu:
      return apps::mojom::LaunchSource::kFromAppListQueryContextMenu;
    case LaunchSource::kFromAppListRecommendation:
      return apps::mojom::LaunchSource::kFromAppListRecommendation;
    case LaunchSource::kFromParentalControls:
      return apps::mojom::LaunchSource::kFromParentalControls;
    case LaunchSource::kFromShelf:
      return apps::mojom::LaunchSource::kFromShelf;
    case LaunchSource::kFromFileManager:
      return apps::mojom::LaunchSource::kFromFileManager;
    case LaunchSource::kFromLink:
      return apps::mojom::LaunchSource::kFromLink;
    case LaunchSource::kFromOmnibox:
      return apps::mojom::LaunchSource::kFromOmnibox;
    case LaunchSource::kFromChromeInternal:
      return apps::mojom::LaunchSource::kFromChromeInternal;
    case LaunchSource::kFromKeyboard:
      return apps::mojom::LaunchSource::kFromKeyboard;
    case LaunchSource::kFromOtherApp:
      return apps::mojom::LaunchSource::kFromOtherApp;
    case LaunchSource::kFromMenu:
      return apps::mojom::LaunchSource::kFromMenu;
    case LaunchSource::kFromInstalledNotification:
      return apps::mojom::LaunchSource::kFromInstalledNotification;
    case LaunchSource::kFromTest:
      return apps::mojom::LaunchSource::kFromTest;
    case LaunchSource::kFromArc:
      return apps::mojom::LaunchSource::kFromArc;
    case LaunchSource::kFromSharesheet:
      return apps::mojom::LaunchSource::kFromSharesheet;
    case LaunchSource::kFromReleaseNotesNotification:
      return apps::mojom::LaunchSource::kFromReleaseNotesNotification;
    case LaunchSource::kFromFullRestore:
      return apps::mojom::LaunchSource::kFromFullRestore;
    case LaunchSource::kFromSmartTextContextMenu:
      return apps::mojom::LaunchSource::kFromSmartTextContextMenu;
    case LaunchSource::kFromDiscoverTabNotification:
      return apps::mojom::LaunchSource::kFromDiscoverTabNotification;
    case LaunchSource::kFromManagementApi:
      return apps::mojom::LaunchSource::kFromManagementApi;
    case LaunchSource::kFromKiosk:
      return apps::mojom::LaunchSource::kFromKiosk;
    case LaunchSource::kFromCommandLine:
      return apps::mojom::LaunchSource::kFromCommandLine;
    case LaunchSource::kFromBackgroundMode:
      return apps::mojom::LaunchSource::kFromBackgroundMode;
    case LaunchSource::kFromNewTabPage:
      return apps::mojom::LaunchSource::kFromNewTabPage;
    case LaunchSource::kFromIntentUrl:
      return apps::mojom::LaunchSource::kFromIntentUrl;
    case LaunchSource::kFromOsLogin:
      return apps::mojom::LaunchSource::kFromOsLogin;
    case LaunchSource::kFromProtocolHandler:
      return apps::mojom::LaunchSource::kFromProtocolHandler;
    case LaunchSource::kFromUrlHandler:
      return apps::mojom::LaunchSource::kFromUrlHandler;
  }
}

WindowInfoPtr ConvertMojomWindowInfoToWindowInfo(
    const apps::mojom::WindowInfoPtr& mojom_window_info) {
  if (!mojom_window_info) {
    return nullptr;
  }

  auto window_info = std::make_unique<WindowInfo>();
  window_info->window_id = mojom_window_info->window_id;
  window_info->state = mojom_window_info->state;
  window_info->display_id = mojom_window_info->display_id;
  if (mojom_window_info->bounds) {
    window_info->bounds = gfx::Rect{
        mojom_window_info->bounds->x, mojom_window_info->bounds->y,
        mojom_window_info->bounds->width, mojom_window_info->bounds->height};
  }
  return window_info;
}

apps::mojom::WindowInfoPtr ConvertWindowInfoToMojomWindowInfo(
    const WindowInfoPtr& window_info) {
  if (!window_info) {
    return nullptr;
  }

  auto mojom_window_info = apps::mojom::WindowInfo::New();
  mojom_window_info->window_id = window_info->window_id;
  mojom_window_info->state = window_info->state;
  mojom_window_info->display_id = window_info->display_id;
  if (window_info->bounds.has_value()) {
    auto mojom_rect = apps::mojom::Rect::New();
    mojom_rect->x = window_info->bounds->x();
    mojom_rect->y = window_info->bounds->y();
    mojom_rect->width = window_info->bounds->width();
    mojom_rect->height = window_info->bounds->height();
    mojom_window_info->bounds = std::move(mojom_rect);
  }
  return mojom_window_info;
}

}  // namespace apps
