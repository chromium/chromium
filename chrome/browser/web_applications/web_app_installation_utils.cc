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
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
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

std::vector<IconSizes> GetDownloadedShortcutsMenuIconsSizes(
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  std::vector<IconSizes> shortcuts_menu_icons_sizes;
  shortcuts_menu_icons_sizes.reserve(shortcuts_menu_icon_bitmaps.size());
  for (const auto& shortcut_icon_bitmaps : shortcuts_menu_icon_bitmaps) {
    IconSizes icon_sizes;
    icon_sizes.SetSizesForPurpose(IconPurpose::ANY,
                                  GetSquareSizePxs(shortcut_icon_bitmaps.any));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MASKABLE,
        GetSquareSizePxs(shortcut_icon_bitmaps.maskable));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MONOCHROME,
        GetSquareSizePxs(shortcut_icon_bitmaps.monochrome));
    shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
  }
  return shortcuts_menu_icons_sizes;
}

}  // namespace

void SetWebAppManifestFields(const WebAppInstallInfo& web_app_info,
                             WebApp& web_app) {
  DCHECK(!web_app_info.title.empty());
  web_app.SetName(base::UTF16ToUTF8(web_app_info.title));

  if (base::FeatureList::IsEnabled(blink::features::kWebAppEnableManifestId)) {
    web_app.SetStartUrl(web_app_info.start_url);
    web_app.SetManifestId(web_app_info.manifest_id);
  }
  web_app.SetDisplayMode(web_app_info.display_mode);
  web_app.SetDisplayModeOverride(web_app_info.display_override);

  web_app.SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app.SetLaunchQueryParams(web_app_info.launch_query_params);
  web_app.SetScope(web_app_info.scope);
  DCHECK(!web_app_info.theme_color.has_value() ||
         SkColorGetA(*web_app_info.theme_color) == SK_AlphaOPAQUE);
  web_app.SetThemeColor(web_app_info.theme_color);

  DCHECK(!web_app_info.dark_mode_theme_color.has_value() ||
         SkColorGetA(*web_app_info.dark_mode_theme_color) == SK_AlphaOPAQUE);
  web_app.SetDarkModeThemeColor(web_app_info.dark_mode_theme_color);

  DCHECK(!web_app_info.background_color.has_value() ||
         SkColorGetA(*web_app_info.background_color) == SK_AlphaOPAQUE);
  web_app.SetBackgroundColor(web_app_info.background_color);

  DCHECK(!web_app_info.dark_mode_background_color.has_value() ||
         SkColorGetA(*web_app_info.dark_mode_background_color) ==
             SK_AlphaOPAQUE);
  web_app.SetDarkModeBackgroundColor(web_app_info.dark_mode_background_color);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = base::UTF16ToUTF8(web_app_info.title);
  sync_fallback_data.theme_color = web_app_info.theme_color;
  sync_fallback_data.scope = web_app_info.scope;
  sync_fallback_data.icon_infos = web_app_info.manifest_icons;
  web_app.SetSyncFallbackData(std::move(sync_fallback_data));

  web_app.SetManifestIcons(web_app_info.manifest_icons);
  web_app.SetDownloadedIconSizes(
      IconPurpose::ANY, GetSquareSizePxs(web_app_info.icon_bitmaps.any));
  web_app.SetDownloadedIconSizes(
      IconPurpose::MASKABLE,
      GetSquareSizePxs(web_app_info.icon_bitmaps.maskable));
  web_app.SetDownloadedIconSizes(
      IconPurpose::MONOCHROME,
      GetSquareSizePxs(web_app_info.icon_bitmaps.monochrome));
  web_app.SetIsGeneratedIcon(web_app_info.is_generated_icon);

  web_app.SetStorageIsolated(web_app_info.is_storage_isolated);
  web_app.SetPermissionsPolicy(web_app_info.permissions_policy);

  web_app.SetShortcutsMenuItemInfos(web_app_info.shortcuts_menu_item_infos);
  web_app.SetDownloadedShortcutsMenuIconsSizes(
      GetDownloadedShortcutsMenuIconsSizes(
          web_app_info.shortcuts_menu_icon_bitmaps));

  if (web_app.file_handler_approval_state() == ApiApprovalState::kAllowed &&
      !AreNewFileHandlersASubsetOfOld(web_app.file_handlers(),
                                      web_app_info.file_handlers)) {
    web_app.SetFileHandlerApprovalState(ApiApprovalState::kRequiresPrompt);
  }
  web_app.SetFileHandlers(web_app_info.file_handlers);
  web_app.SetShareTarget(web_app_info.share_target);
  web_app.SetProtocolHandlers(web_app_info.protocol_handlers);
  web_app.SetUrlHandlers(web_app_info.url_handlers);
  web_app.SetNoteTakingNewNoteUrl(web_app_info.note_taking_new_note_url);

  web_app.SetCaptureLinks(web_app_info.capture_links);

  web_app.SetHandleLinks(web_app_info.handle_links);

  web_app.SetManifestUrl(web_app_info.manifest_url);

  web_app.SetLaunchHandler(web_app_info.launch_handler);
}

void MaybeDisableOsIntegration(const WebAppRegistrar* app_registrar,
                               const AppId& app_id,
                               InstallOsHooksOptions* options) {
#if !BUILDFLAG(IS_CHROMEOS)  // Deeper OS integration is expected on ChromeOS.
  DCHECK(app_registrar);

  // Disable OS integration if the app was installed by default only, and not
  // through any other means like an enterprise policy or store.
  if (app_registrar->WasInstalledByDefaultOnly(app_id)) {
    options->add_to_desktop = false;
    options->add_to_quick_launch_bar = false;
    options->os_hooks[OsHookType::kShortcuts] = false;
    options->os_hooks[OsHookType::kRunOnOsLogin] = false;
    options->os_hooks[OsHookType::kShortcutsMenu] = false;
    options->os_hooks[OsHookType::kUninstallationViaOsSettings] = false;
    options->os_hooks[OsHookType::kFileHandlers] = false;
    options->os_hooks[OsHookType::kProtocolHandlers] = false;
    options->os_hooks[OsHookType::kUrlHandlers] = false;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

bool CanWebAppUpdateIdentity(const WebApp* web_app) {
  if (web_app->IsPolicyInstalledApp() &&
      base::FeatureList::IsEnabled(
          features::kWebAppManifestPolicyAppIdentityUpdate)) {
    return true;
  }
  return web_app->IsPreinstalledApp();
}

}  // namespace web_app
