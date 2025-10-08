// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_SETTINGS_UI_SETTINGS_APP_MANAGER_H_
#define CHROMEOS_ASH_EXPERIENCES_SETTINGS_UI_SETTINGS_APP_MANAGER_H_

#include <string_view>

#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/component_export.h"
#include "ui/display/types/display_constants.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

// Manages Settings System Web App interaction.
// This (its implementation class) can be alive at most once at a time.
class COMPONENT_EXPORT(SETTINGS_UI) SettingsAppManager {
 public:
  // Returns the singleton instance
  static SettingsAppManager* Get();

  SettingsAppManager(const SettingsAppManager&) = delete;
  SettingsAppManager& operator=(const SettingsAppManager&) = delete;

  struct OpenParams {
    // If specified, opens the subpage, instead of settings top page.
    std::string_view sub_page;
    std::optional<chromeos::settings::mojom::Setting> setting_id;

    // ID of the display to be used, if given.
    int64_t display_id = display::kInvalidDisplayId;
  };
  // Opens the settings app of the given `user`.
  virtual void Open(const user_manager::User& user, OpenParams params) = 0;

 protected:
  SettingsAppManager();
  virtual ~SettingsAppManager();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_SETTINGS_UI_SETTINGS_APP_MANAGER_H_
