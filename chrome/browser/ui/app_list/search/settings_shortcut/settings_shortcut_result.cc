// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_result.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_metadata.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

// TODO(wutao): Need UX specs on these values.
constexpr int kSettingsIconSize = 48;

// Icon color.
constexpr SkColor kSettingsColor = SkColorSetARGB(0x8A, 0x00, 0x00, 0x00);

}  // namespace

SettingsShortcutResult::SettingsShortcutResult(
    Profile* profile,
    const SettingsShortcut& settings_shortcut)
    : profile_(profile), settings_shortcut_(settings_shortcut) {
  set_id(settings_shortcut.shortcut_id);
  SetTitle(
      l10n_util::GetStringUTF16(settings_shortcut.name_string_resource_id));
  // TODO(wutao): create a new display type kSettingsShortcut.
  SetDisplayType(DisplayType::kTile);
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::CreateVectorIcon(settings_shortcut.vector_icon, kSettingsColor),
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kSettingsIconSize, kSettingsIconSize)));
}

void SettingsShortcutResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, settings_shortcut_.subpage);
}

void SettingsShortcutResult::GetContextMenuModel(
    GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

ash::SearchResultType SettingsShortcutResult::GetSearchResultType() const {
  return ash::SETTINGS_SHORTCUT;
}

}  // namespace app_list
