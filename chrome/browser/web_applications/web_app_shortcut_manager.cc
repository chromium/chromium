// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_shortcut_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep_default.h"

namespace web_app {

WebAppShortcutManager::WebAppShortcutManager(
    Profile* profile,
    WebAppIconManager* icon_manager,
    FileHandlerManager* file_handler_manager)
    : AppShortcutManager(profile),
      icon_manager_(icon_manager),
      file_handler_manager_(file_handler_manager) {}

WebAppShortcutManager::~WebAppShortcutManager() = default;

std::unique_ptr<ShortcutInfo> WebAppShortcutManager::BuildShortcutInfo(
    const AppId& app_id) {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  DCHECK(app);
  return BuildShortcutInfoForWebApp(app);
}

void WebAppShortcutManager::GetShortcutInfoForApp(
    const AppId& app_id,
    GetShortcutInfoCallback callback) {
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);

  // app could be nullptr if registry profile is being deleted.
  if (!app) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Build a common intersection between desired and downloaded icons.
  auto icon_sizes_in_px = base::STLSetIntersection<std::vector<SquareSizePx>>(
      app->downloaded_icon_sizes(IconPurpose::ANY),
      GetDesiredIconSizesForShortcut());

  DCHECK(icon_manager_);
  if (!icon_sizes_in_px.empty()) {
    icon_manager_->ReadIcons(app_id, IconPurpose::ANY, icon_sizes_in_px,
                             base::BindOnce(&WebAppShortcutManager::OnIconsRead,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            app_id, std::move(callback)));
    return;
  }

  // If there is no single icon at the desired sizes, we will resize what we can
  // get.
  SquareSizePx desired_icon_size = GetDesiredIconSizesForShortcut().back();

  icon_manager_->ReadIconAndResize(
      app_id, IconPurpose::ANY, desired_icon_size,
      base::BindOnce(&WebAppShortcutManager::OnIconsRead,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void WebAppShortcutManager::OnIconsRead(
    const AppId& app_id,
    GetShortcutInfoCallback callback,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  // |icon_bitmaps| can be empty here if no icon found.
  const WebApp* app = GetWebAppRegistrar().GetAppById(app_id);
  if (!app) {
    std::move(callback).Run(nullptr);
    return;
  }

  gfx::ImageFamily image_family;
  for (auto& size_and_bitmap : icon_bitmaps) {
    image_family.Add(gfx::ImageSkia(
        gfx::ImageSkiaRep(size_and_bitmap.second, /*scale=*/0.0f)));
  }

  // If the image failed to load, use the standard application icon.
  if (image_family.empty()) {
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
  }

  std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfoForWebApp(app);
  shortcut_info->favicon = std::move(image_family);

  std::move(callback).Run(std::move(shortcut_info));
}

std::unique_ptr<ShortcutInfo> WebAppShortcutManager::BuildShortcutInfoForWebApp(
    const WebApp* app) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();

  shortcut_info->extension_id = app->app_id();
  shortcut_info->url = app->start_url();
  shortcut_info->title = base::UTF8ToUTF16(app->name());
  shortcut_info->description = base::UTF8ToUTF16(app->description());
  shortcut_info->profile_path = profile()->GetPath();
  shortcut_info->profile_name =
      profile()->GetPrefs()->GetString(prefs::kProfileName);
  shortcut_info->is_multi_profile = true;

  if (const apps::FileHandlers* file_handlers =
          file_handler_manager_->GetEnabledFileHandlers(app->app_id())) {
    shortcut_info->file_handler_extensions =
        GetFileExtensionsFromFileHandlers(*file_handlers);
    shortcut_info->file_handler_mime_types =
        GetMimeTypesFromFileHandlers(*file_handlers);
  }

  return shortcut_info;
}

WebAppRegistrar& WebAppShortcutManager::GetWebAppRegistrar() {
  WebAppRegistrar* web_app_registrar = registrar()->AsWebAppRegistrar();
  DCHECK(web_app_registrar);
  return *web_app_registrar;
}

}  // namespace web_app
