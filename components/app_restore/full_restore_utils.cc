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

std::unique_ptr<app_restore::WindowInfo> GetWindowInfo(aura::Window* window) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return nullptr;

  return FullRestoreReadHandler::GetInstance()->GetWindowInfo(window);
}

int32_t FetchRestoreWindowId(const std::string& app_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return 0;

  // `DeskTemplateReadHandler::FetchRestoreWindowId()` will return 0 if full
  // restore is running.
  // TODO(sammiequon): Separate full restore and desk templates logic.
  const int32_t desk_template_restore_window_id =
      app_restore::DeskTemplateReadHandler::GetInstance()->FetchRestoreWindowId(
          app_id);
  if (desk_template_restore_window_id > 0)
    return desk_template_restore_window_id;

  return FullRestoreReadHandler::GetInstance()->FetchRestoreWindowId(app_id);
}

int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return 0;

  return FullRestoreReadHandler::GetInstance()->GetArcRestoreWindowIdForTaskId(
      task_id);
}

int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return 0;

  return FullRestoreReadHandler::GetInstance()
      ->GetArcRestoreWindowIdForSessionId(session_id);
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

void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return;

  FullRestoreReadHandler::GetInstance()->ModifyWidgetParams(restore_window_id,
                                                            out_params);
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

void SetArcConnection(bool is_connection_ready) {
  FullRestoreSaveHandler::GetInstance()->SetArcConnection(is_connection_ready);
}

void OnTaskThemeColorUpdated(int32_t task_id,
                             uint32_t primary_color,
                             uint32_t status_bar_color) {
  FullRestoreSaveHandler::GetInstance()->OnTaskThemeColorUpdated(
      task_id, primary_color, status_bar_color);
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
