// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_utils.h"

#include "ash/public/cpp/ash_features.h"
#include "base/files/file_path.h"
#include "components/account_id/account_id.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/window_info.h"

namespace full_restore {

DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowIdKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kRestoreWindowIdKey, 0)

void SaveAppLaunchInfo(const base::FilePath& profile_dir,
                       std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!ash::features::IsFullRestoreEnabled() || !app_launch_info)
    return;

  FullRestoreSaveHandler::GetInstance()->SaveAppLaunchInfo(
      profile_dir, std::move(app_launch_info));
}

void SaveWindowInfo(const WindowInfo& window_info) {
  if (!ash::features::IsFullRestoreEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->SaveWindowInfo(window_info);
}

std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window) {
  if (!ash::features::IsFullRestoreEnabled())
    return nullptr;

  auto window_info = std::make_unique<WindowInfo>();

  // TODO(crbug.com/1146900): Get the window information from the full restore
  // file.
  return window_info;
}

bool ShouldRestore(const AccountId& account_id) {
  return FullRestoreInfo::GetInstance()->ShouldRestore(account_id);
}

}  // namespace full_restore
