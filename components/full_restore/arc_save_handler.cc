// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/arc_save_handler.h"

#include "base/containers/contains.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/window_info.h"
#include "ui/aura/window.h"

namespace full_restore {

ArcSaveHandler::ArcSaveHandler(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

ArcSaveHandler::~ArcSaveHandler() = default;

void ArcSaveHandler::SaveAppLaunchInfo(AppLaunchInfoPtr app_launch_info) {
  DCHECK(app_launch_info->arc_session_id.has_value());

  // Save |app_launch_info| to |session_id_to_app_launch_info_|, and wait for
  // the ARC task to be created.
  int32_t session_id = app_launch_info->arc_session_id.value();
  session_id_to_app_launch_info_[session_id] = std::move(app_launch_info);
}

void ArcSaveHandler::OnWindowInitialized(aura::Window* window) {
  int32_t task_id = window->GetProperty(::full_restore::kWindowIdKey);
  if (!base::Contains(task_id_to_app_id_, task_id))
    return;

  // If the task has been created, call OnAppLaunched to save the window
  // information.
  FullRestoreInfo::GetInstance()->OnAppLaunched(window);
}

void ArcSaveHandler::OnWindowDestroyed(aura::Window* window) {
  int32_t task_id = window->GetProperty(::full_restore::kWindowIdKey);

  auto task_it = task_id_to_app_id_.find(task_id);
  if (task_it == task_id_to_app_id_.end())
    return;

  FullRestoreSaveHandler::GetInstance()->RemoveWindowInfo(
      profile_path_, task_it->second, task_id);
}

void ArcSaveHandler::OnTaskCreated(const std::string& app_id,
                                   int32_t task_id,
                                   int32_t session_id) {
  auto it = session_id_to_app_launch_info_.find(session_id);
  if (it == session_id_to_app_launch_info_.end())
    return;

  task_id_to_app_id_[task_id] = app_id;

  auto app_launch_info = std::move(it->second);
  session_id_to_app_launch_info_.erase(it);

  app_launch_info->window_id = task_id;
  FullRestoreSaveHandler::GetInstance()->AddAppLaunchInfo(
      profile_path_, std::move(app_launch_info));
}

void ArcSaveHandler::OnTaskDestroyed(int32_t task_id) {
  auto it = task_id_to_app_id_.find(task_id);
  if (it == task_id_to_app_id_.end())
    return;

  FullRestoreSaveHandler::GetInstance()->RemoveAppRestoreData(
      profile_path_, it->second, task_id);

  task_id_to_app_id_.erase(task_id);
}

int32_t ArcSaveHandler::GetArcSessionId() {
  if (session_id_ >= kArcSessionIdOffsetForRestoredLaunching) {
    LOG(WARNING) << "ARC session id is too large: " << session_id_;
    session_id_ = 0;
  }

  return ++session_id_;
}

}  // namespace full_restore
