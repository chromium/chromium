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
  }
}

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
    case apps::mojom::LaunchSource::kFromLockScreen:
      return LaunchSource::kFromLockScreen;
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
    case LaunchSource::kFromLockScreen:
      return apps::mojom::LaunchSource::kFromLockScreen;
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
