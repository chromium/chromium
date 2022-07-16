// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/full_restore_utils.h"

#include "base/files/file_path.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_info.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_info.h"

namespace full_restore {

void SaveAppLaunchInfo(
    const base::FilePath& profile_path,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info) {
  if (!full_restore::features::IsFullRestoreEnabled() || !app_launch_info)
    return;

  FullRestoreSaveHandler::GetInstance()->SaveAppLaunchInfo(
      profile_path, std::move(app_launch_info));
}

void SaveWindowInfo(const app_restore::WindowInfo& window_info) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->SaveWindowInfo(window_info);
}

void SetActiveProfilePath(const base::FilePath& profile_path) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->SetActiveProfilePath(profile_path);
  FullRestoreReadHandler::GetInstance()->SetActiveProfilePath(profile_path);
}

bool HasAppTypeBrowser(const base::FilePath& profile_path) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return false;

  return FullRestoreReadHandler::GetInstance()->HasAppTypeBrowser(profile_path);
}

bool HasBrowser(const base::FilePath& profile_path) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return false;

  return FullRestoreReadHandler::GetInstance()->HasBrowser(profile_path);
}

bool HasWindowInfo(int32_t restore_window_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return false;

  return FullRestoreReadHandler::GetInstance()->HasWindowInfo(
      restore_window_id);
}

void AddChromeBrowserLaunchInfoForTesting(const base::FilePath& profile_path) {
  FullRestoreReadHandler::GetInstance()
      ->AddChromeBrowserLaunchInfoForTesting(  // IN-TEST
          profile_path);
}

std::string GetAppId(aura::Window* window) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return std::string();

  return FullRestoreSaveHandler::GetInstance()->GetAppId(window);
}

}  // namespace full_restore
