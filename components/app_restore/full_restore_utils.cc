// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/full_restore_utils.h"

#include "base/files/file_path.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_info.h"

namespace full_restore {

void SaveAppLaunchInfo(
    const base::FilePath& profile_path,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  FullRestoreSaveHandler::GetInstance()->SaveAppLaunchInfo(
      profile_path, std::move(app_launch_info));
}

void SaveWindowInfo(const app_restore::WindowInfo& window_info) {
  FullRestoreSaveHandler::GetInstance()->SaveWindowInfo(window_info);
}

void SetActiveProfilePath(const base::FilePath& profile_path) {
  FullRestoreSaveHandler::GetInstance()->SetActiveProfilePath(profile_path);
  FullRestoreReadHandler::GetInstance()->SetActiveProfilePath(profile_path);
}

void SetPrimaryProfilePath(const base::FilePath& profile_path) {
  FullRestoreSaveHandler::GetInstance()->SetPrimaryProfilePath(profile_path);
  FullRestoreReadHandler::GetInstance()->SetPrimaryProfilePath(profile_path);
}

bool HasAppTypeBrowser(const base::FilePath& profile_path) {
  return FullRestoreReadHandler::GetInstance()->HasAppTypeBrowser(profile_path);
}

bool HasBrowser(const base::FilePath& profile_path) {
  return FullRestoreReadHandler::GetInstance()->HasBrowser(profile_path);
}

void AddChromeBrowserLaunchInfoForTesting(const base::FilePath& profile_path) {
  FullRestoreReadHandler::GetInstance()
      ->AddChromeBrowserLaunchInfoForTesting(  // IN-TEST
          profile_path);
}

std::string GetAppId(aura::Window* window) {
  return FullRestoreSaveHandler::GetInstance()->GetAppId(window);
}

void OnLacrosChromeAppWindowAdded(const std::string& app_id,
                                  const std::string& window_id) {
  if (!full_restore::features::IsFullRestoreForLacrosEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->OnLacrosChromeAppWindowAdded(
      app_id, window_id);
}

void OnLacrosChromeAppWindowRemoved(const std::string& app_id,
                                    const std::string& window_id) {
  if (!full_restore::features::IsFullRestoreForLacrosEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->OnLacrosChromeAppWindowRemoved(
      app_id, window_id);
}

void SaveRemovingDeskGuid(const base::Uuid& removing_desk_guid) {
  FullRestoreSaveHandler::GetInstance()->SaveRemovingDeskGuid(
      removing_desk_guid);
}

void ResetRemovingDeskGuid() {
  FullRestoreSaveHandler::GetInstance()->SaveRemovingDeskGuid(base::Uuid());
}

}  // namespace full_restore
