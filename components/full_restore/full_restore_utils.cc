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

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(FULL_RESTORE), int32_t*)

namespace full_restore {

DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowIdKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kRestoreWindowIdKey, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIdKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(int32_t, kActivationIndexKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kParentToHiddenContainerKey, false)

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

int32_t GetArcRestoreWindowId(int32_t task_id) {
  if (!ash::features::IsFullRestoreEnabled())
    return 0;

  return FullRestoreReadHandler::GetInstance()->GetArcRestoreWindowId(task_id);
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

  return FullRestoreReadHandler::GetInstance()->HasWindowInfo(
      restore_window_id);
}

bool ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params) {
  if (!ash::features::IsFullRestoreEnabled())
    return false;

  return FullRestoreReadHandler::GetInstance()->ModifyWidgetParams(
      restore_window_id, out_params);
}

void OnTaskCreated(const std::string& app_id,
                   int32_t task_id,
                   int32_t session_id) {
  FullRestoreReadHandler::GetInstance()->OnTaskCreated(app_id, task_id,
                                                       session_id);
  FullRestoreSaveHandler::GetInstance()->OnTaskCreated(app_id, task_id,
                                                       session_id);
}

void OnTaskDestroyed(int32_t task_id) {
  FullRestoreReadHandler::GetInstance()->OnTaskDestroyed(task_id);
  FullRestoreSaveHandler::GetInstance()->OnTaskDestroyed(task_id);
}

}  // namespace full_restore
