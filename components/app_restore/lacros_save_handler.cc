// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/lacros_save_handler.h"

#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_info.h"
#include "ui/aura/window.h"

namespace full_restore {

LacrosSaveHandler::LacrosSaveHandler(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

LacrosSaveHandler::~LacrosSaveHandler() = default;

void LacrosSaveHandler::OnWindowInitialized(aura::Window* window) {
  const std::string lacros_window_id = app_restore::GetLacrosWindowId(window);

  // If `window` has been saved by OnBrowserWindowAdded, we don't need to save
  // again.
  if (base::Contains(window_candidates_, lacros_window_id))
    return;

  std::string app_id;
  int32_t window_id = ++window_id_;
  std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info;

  auto it = lacros_window_id_to_app_id_.find(lacros_window_id);
  if (it != lacros_window_id_to_app_id_.end()) {
    // For Chrome app windows, get the app launch info and set the Chrome app
    // id.
    app_id = it->second;
    app_launch_info = FullRestoreSaveHandler::GetInstance()->FetchAppLaunchInfo(
        profile_path_, app_id);
    app_launch_info->window_id = window_id;
  } else {
    app_id = app_constants::kLacrosAppId;
    app_launch_info =
        std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);
  }

  window_candidates_[lacros_window_id].app_id = app_id;
  window_candidates_[lacros_window_id].window_id = window_id;

  FullRestoreSaveHandler::GetInstance()->AddAppLaunchInfo(
      profile_path_, std::move(app_launch_info));
}

void LacrosSaveHandler::OnWindowDestroyed(aura::Window* window) {
  const std::string lacros_window_id = app_restore::GetLacrosWindowId(window);
  lacros_window_id_to_app_id_.erase(lacros_window_id);

  auto it = window_candidates_.find(lacros_window_id);
  if (it == window_candidates_.end())
    return;

  FullRestoreSaveHandler::GetInstance()->RemoveAppRestoreData(
      profile_path_, it->second.app_id, it->second.window_id);

  window_candidates_.erase(it);
}

void LacrosSaveHandler::OnBrowserWindowAdded(aura::Window* const window,
                                             uint32_t browser_session_id) {
  const std::string lacros_window_id = app_restore::GetLacrosWindowId(window);
  std::unique_ptr<app_restore::WindowInfo> window_info;
  auto* save_handler = FullRestoreSaveHandler::GetInstance();
  DCHECK(save_handler);

  auto it = window_candidates_.find(lacros_window_id);
  if (it != window_candidates_.end() &&
      it->second.window_id != browser_session_id) {
    // If the window has been created and saved using different window id, get
    // the current window info, then remove the restore data for the old window
    // id, and re-save the restore data with the new `browser_session_id`.
    window_info = save_handler->GetWindowInfo(profile_path_, it->second.app_id,
                                              it->second.window_id);

    save_handler->RemoveAppRestoreData(profile_path_, it->second.app_id,
                                       it->second.window_id);
  }

  window_candidates_[lacros_window_id].app_id = app_constants::kLacrosAppId;
  window_candidates_[lacros_window_id].window_id = browser_session_id;

  save_handler->AddAppLaunchInfo(
      profile_path_, std::make_unique<app_restore::AppLaunchInfo>(
                         app_constants::kLacrosAppId, browser_session_id));

  if (window_info) {
    save_handler->ModifyWindowInfo(profile_path_, app_constants::kLacrosAppId,
                                   browser_session_id, *window_info);
  }
}

void LacrosSaveHandler::OnAppWindowAdded(const std::string& app_id,
                                         const std::string& lacros_window_id) {
  auto it = window_candidates_.find(lacros_window_id);
  if (it == window_candidates_.end()) {
    // If the window is not created yet, save the app id to
    // `lacros_window_id_to_app_id_` to wait for the window.
    lacros_window_id_to_app_id_[lacros_window_id] = app_id;
    return;
  }

  // If the window has been created, get the app launch info and the current
  // window info, then remove the restore data for lacros browser app id, and
  // re-save the restore data for the Chrome app id.

  auto* save_handler = FullRestoreSaveHandler::GetInstance();
  DCHECK(save_handler);
  auto window_info = save_handler->GetWindowInfo(
      profile_path_, it->second.app_id, it->second.window_id);

  save_handler->RemoveAppRestoreData(profile_path_, it->second.app_id,
                                     it->second.window_id);

  it->second.app_id = app_id;
  auto app_launch_info =
      save_handler->FetchAppLaunchInfo(profile_path_, app_id);
  app_launch_info->window_id = it->second.window_id;

  save_handler->AddAppLaunchInfo(profile_path_, std::move(app_launch_info));
  save_handler->ModifyWindowInfo(profile_path_, app_id, it->second.window_id,
                                 *window_info);
}

void LacrosSaveHandler::OnAppWindowRemoved(
    const std::string& app_id,
    const std::string& lacros_window_id) {
  lacros_window_id_to_app_id_.erase(lacros_window_id);
}

void LacrosSaveHandler::ModifyWindowInfo(
    const app_restore::WindowInfo& window_info) {
  auto it = window_candidates_.find(
      app_restore::GetLacrosWindowId(window_info.window));
  if (it != window_candidates_.end()) {
    FullRestoreSaveHandler::GetInstance()->ModifyWindowInfo(
        profile_path_, it->second.app_id, it->second.window_id, window_info);
  }
}

std::string LacrosSaveHandler::GetAppId(aura::Window* window) {
  auto it = window_candidates_.find(app_restore::GetLacrosWindowId(window));
  return it != window_candidates_.end() ? it->second.app_id : std::string();
}

}  // namespace full_restore
