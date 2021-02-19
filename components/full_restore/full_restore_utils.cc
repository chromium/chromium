// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_utils.h"

#include "ash/public/cpp/ash_features.h"
#include "base/files/file_path.h"
#include "components/account_id/account_id.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/window_info.h"
#include "ui/aura/client/aura_constants.h"

namespace full_restore {

DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowIdKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kRestoreWindowIdKey, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIdKey, nullptr)

void SaveAppLaunchInfo(const base::FilePath& profile_path,
                       std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!ash::features::IsFullRestoreEnabled() || !app_launch_info)
    return;

  FullRestoreSaveHandler::GetInstance()->SaveAppLaunchInfo(
      profile_path, std::move(app_launch_info));
}

void SaveWindowInfo(const WindowInfo& window_info) {
  if (!ash::features::IsFullRestoreEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->SaveWindowInfo(window_info);
}

std::unique_ptr<WindowInfo> GetWindowInfo(int32_t restore_window_id) {
  if (!ash::features::IsFullRestoreEnabled())
    return nullptr;

  return FullRestoreReadHandler::GetInstance()->GetWindowInfo(
      restore_window_id);
}

std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window) {
  if (!ash::features::IsFullRestoreEnabled())
    return nullptr;

  return FullRestoreReadHandler::GetInstance()->GetWindowInfo(window);
}

int32_t FetchRestoreWindowId(const std::string& app_id) {
  if (!ash::features::IsFullRestoreEnabled())
    return 0;

  return FullRestoreReadHandler::GetInstance()->FetchRestoreWindowId(app_id);
}

bool ShouldRestore(const AccountId& account_id) {
  return FullRestoreInfo::GetInstance()->ShouldRestore(account_id);
}

void SetActiveProfilePath(const base::FilePath& profile_path) {
  if (!ash::features::IsFullRestoreEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->SetActiveProfilePath(profile_path);
  FullRestoreReadHandler::GetInstance()->SetActiveProfilePath(profile_path);
}

bool HasWindowInfo(int32_t restore_window_id) {
  if (!ash::features::IsFullRestoreEnabled())
    return false;

  std::unique_ptr<WindowInfo> window_info = GetWindowInfo(restore_window_id);
  return !!window_info;
}

void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params) {
  DCHECK(out_params);

  if (!ash::features::IsFullRestoreEnabled())
    return;

  std::unique_ptr<WindowInfo> window_info = GetWindowInfo(restore_window_id);
  if (!window_info)
    return;

  if (window_info->desk_id)
    out_params->workspace = base::NumberToString(*window_info->desk_id);
  out_params->visible_on_all_workspaces =
      window_info->visible_on_all_workspaces.has_value();
  if (window_info->current_bounds)
    out_params->bounds = *window_info->current_bounds;
  if (window_info->restore_bounds) {
    out_params->init_properties_container.SetProperty(
        aura::client::kRestoreBoundsKey, *window_info->restore_bounds);
  }
  if (window_info->window_state_type) {
    // ToWindowShowState will make us lose some ash-specific types (left/right
    // snap). Ash is responsible for restoring these states by checking
    // GetWindowInfo.
    out_params->show_state =
        chromeos::ToWindowShowState(*window_info->window_state_type);
  }
}

}  // namespace full_restore
