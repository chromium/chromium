// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_installation_utils.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

std::vector<SquareSizePx> GetSquareSizePxs(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  std::vector<SquareSizePx> sizes;
  sizes.reserve(icon_bitmaps.size());
  for (const std::pair<const SquareSizePx, SkBitmap>& item : icon_bitmaps)
    sizes.push_back(item.first);
  return sizes;
}

std::vector<std::vector<SquareSizePx>> GetDownloadedShortcutsMenuIconsSizes(
    const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps) {
  std::vector<std::vector<SquareSizePx>> shortcuts_menu_icons_sizes;
  shortcuts_menu_icons_sizes.reserve(shortcuts_menu_icons_bitmaps.size());
  for (const auto& shortcut_icon_bitmaps : shortcuts_menu_icons_bitmaps) {
    shortcuts_menu_icons_sizes.emplace_back(
        GetSquareSizePxs(shortcut_icon_bitmaps));
  }
  return shortcuts_menu_icons_sizes;
}

void SetWebAppFileHandlers(
    const std::vector<blink::Manifest::FileHandler>& manifest_file_handlers,
    WebApp& web_app) {
  apps::FileHandlers web_app_file_handlers;

  for (const auto& manifest_file_handler : manifest_file_handlers) {
    apps::FileHandler web_app_file_handler;
    web_app_file_handler.action = manifest_file_handler.action;

    for (const auto& it : manifest_file_handler.accept) {
      apps::FileHandler::AcceptEntry web_app_accept_entry;
      web_app_accept_entry.mime_type = base::UTF16ToUTF8(it.first);
      for (const auto& manifest_file_extension : it.second)
        web_app_accept_entry.file_extensions.insert(
            base::UTF16ToUTF8(manifest_file_extension));
      web_app_file_handler.accept.push_back(std::move(web_app_accept_entry));
    }

    web_app_file_handlers.push_back(std::move(web_app_file_handler));
  }

  web_app.SetFileHandlers(std::move(web_app_file_handlers));
}

void SetWebAppProtocolHandlers(
    const std::vector<blink::Manifest::ProtocolHandler>& protocol_handlers,
    WebApp& web_app) {
  std::vector<apps::ProtocolHandlerInfo> web_app_protocol_handlers;
  for (const auto& handler : protocol_handlers) {
    apps::ProtocolHandlerInfo protocol_handler_info;
    protocol_handler_info.protocol = base::UTF16ToUTF8(handler.protocol);
    protocol_handler_info.url = handler.url;
    web_app_protocol_handlers.push_back(std::move(protocol_handler_info));
  }

  web_app.SetProtocolHandlers(web_app_protocol_handlers);
}

}  // namespace

void SetWebAppManifestFields(const WebApplicationInfo& web_app_info,
                             WebApp& web_app) {
  DCHECK(!web_app_info.title.empty());
  web_app.SetName(base::UTF16ToUTF8(web_app_info.title));

  web_app.SetDisplayMode(web_app_info.display_mode);
  web_app.SetDisplayModeOverride(web_app_info.display_override);

  web_app.SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app.SetLaunchQueryParams(web_app_info.launch_query_params);
  web_app.SetScope(web_app_info.scope);
  DCHECK(!web_app_info.theme_color.has_value() ||
         SkColorGetA(*web_app_info.theme_color) == SK_AlphaOPAQUE);
  web_app.SetThemeColor(web_app_info.theme_color);
  DCHECK(!web_app_info.background_color.has_value() ||
         SkColorGetA(*web_app_info.background_color) == SK_AlphaOPAQUE);
  web_app.SetBackgroundColor(web_app_info.background_color);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = base::UTF16ToUTF8(web_app_info.title);
  sync_fallback_data.theme_color = web_app_info.theme_color;
  sync_fallback_data.scope = web_app_info.scope;
  sync_fallback_data.icon_infos = web_app_info.icon_infos;
  web_app.SetSyncFallbackData(std::move(sync_fallback_data));

  web_app.SetIconInfos(web_app_info.icon_infos);
  web_app.SetDownloadedIconSizes(
      IconPurpose::ANY, GetSquareSizePxs(web_app_info.icon_bitmaps_any));
  // TODO (crbug.com/1114638): Add monochrome icons support.
  web_app.SetDownloadedIconSizes(
      IconPurpose::MASKABLE,
      GetSquareSizePxs(web_app_info.icon_bitmaps_maskable));
  web_app.SetIsGeneratedIcon(web_app_info.is_generated_icon);

  web_app.SetShortcutsMenuItemInfos(web_app_info.shortcuts_menu_item_infos);
  web_app.SetDownloadedShortcutsMenuIconsSizes(
      GetDownloadedShortcutsMenuIconsSizes(
          web_app_info.shortcuts_menu_icons_bitmaps));

  SetWebAppFileHandlers(web_app_info.file_handlers, web_app);
  web_app.SetShareTarget(web_app_info.share_target);
  SetWebAppProtocolHandlers(web_app_info.protocol_handlers, web_app);

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      web_app_info.run_on_os_login) {
    // TODO(crbug.com/1091964): Obtain actual mode, currently set to the
    // default (windowed).
    web_app.SetRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
  }
}

}  // namespace web_app
