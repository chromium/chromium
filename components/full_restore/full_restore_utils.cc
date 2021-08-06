// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_utils.h"

#include "base/files/file_path.h"
#include "components/account_id/account_id.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/desk_template_read_handler.h"
#include "components/full_restore/features.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/window_info.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(FULL_RESTORE), int32_t*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(FULL_RESTORE),
                                       full_restore::WindowInfo*)

namespace full_restore {

DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowIdKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kRestoreWindowIdKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kGhostWindowSessionIdKey, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIdKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kBrowserAppNameKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAppTypeBrowser, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(int32_t, kActivationIndexKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kParentToHiddenContainerKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kLaunchedFromFullRestoreKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRealArcTaskWindow, true)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(WindowInfo, kWindowInfoKey, nullptr)

void SaveAppLaunchInfo(const base::FilePath& profile_path,
                       std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!full_restore::features::IsFullRestoreEnabled() || !app_launch_info)
    return;

  FullRestoreSaveHandler::GetInstance()->SaveAppLaunchInfo(
      profile_path, std::move(app_launch_info));
}

void SaveWindowInfo(const WindowInfo& window_info) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return;

  FullRestoreSaveHandler::GetInstance()->SaveWindowInfo(window_info);
}

std::unique_ptr<AppLaunchInfo> GetArcAppLaunchInfo(const std::string& app_id,
                                                   int32_t session_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return nullptr;

  return FullRestoreReadHandler::GetInstance()->GetArcAppLaunchInfo(app_id,
                                                                    session_id);
}

std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window) {
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
      DeskTemplateReadHandler::GetInstance()->FetchRestoreWindowId(app_id);
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

bool ShouldRestore(const AccountId& account_id) {
  return FullRestoreInfo::GetInstance()->ShouldRestore(account_id);
}

bool CanPerformRestore(const AccountId& account_id) {
  return FullRestoreInfo::GetInstance()->CanPerformRestore(account_id);
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

void OnTaskThemeColorUpdated(int32_t task_id,
                             uint32_t primary_color,
                             uint32_t status_bar_color) {
  FullRestoreSaveHandler::GetInstance()->OnTaskThemeColorUpdated(
      task_id, primary_color, status_bar_color);
}

void AddChromeBrowserLaunchInfoForTesting(const base::FilePath& profile_path) {
  FullRestoreReadHandler::GetInstance()->AddChromeBrowserLaunchInfoForTesting(
      profile_path);
}

std::string GetAppId(aura::Window* window) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return std::string();

  return FullRestoreSaveHandler::GetInstance()->GetAppId(window);
}

}  // namespace full_restore
