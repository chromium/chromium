// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/lacros_read_handler.h"

#include "ash/constants/app_types.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/sessions/core/session_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace app_restore {

LacrosReadHandler::LacrosReadHandler(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

LacrosReadHandler::~LacrosReadHandler() = default;

void LacrosReadHandler::AddRestoreData(const std::string& app_id,
                                       int32_t window_id) {
  restore_window_id_to_app_id_[window_id] = app_id;
}

void LacrosReadHandler::OnLacrosBrowserWindowAdded(
    aura::Window* const window,
    int32_t restored_browser_session_id) {
  if (!IsLacrosWindow(window))
    return;

  auto it = window_to_window_data_.find(window);
  if (it != window_to_window_data_.end() &&
      it->second.restore_window_id == restored_browser_session_id) {
    // `window` has been restored.
    return;
  }

  window_to_window_data_[window].restore_window_id =
      restored_browser_session_id;

  // If there is a restore data, set the app id to restore `window`.
  auto restore_it =
      restore_window_id_to_app_id_.find(restored_browser_session_id);
  if (restore_it != restore_window_id_to_app_id_.end())
    window_to_window_data_[window].app_id = restore_it->second;

  // If `window` is added to a hidden container, call UpdateWindow to restore
  // and remove `window` from the hidden container.
  if (base::Contains(window_candidates_, window))
    UpdateWindow(window);
}

void LacrosReadHandler::OnAppWindowAdded(const std::string& app_id,
                                         const std::string& lacros_window_id) {
  lacros_window_id_to_app_id_[lacros_window_id] = app_id;

  auto window_it =
      std::find_if(window_candidates_.begin(), window_candidates_.end(),
                   [lacros_window_id](aura::Window* window) {
                     return GetLacrosWindowId(window) == lacros_window_id;
                   });
  if (window_it == window_candidates_.end())
    return;

  SetWindowData(
      *window_it, app_id,
      full_restore::FullRestoreReadHandler::GetInstance()->FetchRestoreWindowId(
          app_id));
  UpdateWindow(*window_it);
}

void LacrosReadHandler::OnAppWindowRemoved(
    const std::string& app_id,
    const std::string& lacros_window_id) {
  lacros_window_id_to_app_id_.erase(lacros_window_id);
}

void LacrosReadHandler::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window->GetProperty(app_restore::kParentToHiddenContainerKey)) {
    // If `window` has been removed from the hidden container, we don't need to
    // restore it, because it has been restored.
    return;
  }

  auto window_it = window_to_window_data_.find(window);
  if (window_it != window_to_window_data_.end()) {
    // We have received the restore window, so restore and remove `window` from
    // the hidden container.
    UpdateWindow(window);
    return;
  }

  const auto lacros_window_id = GetLacrosWindowId(window);
  auto it = lacros_window_id_to_app_id_.find(lacros_window_id);
  if (it != lacros_window_id_to_app_id_.end()) {
    // We have received the app id for the Chrome app window, so restore and
    // remove `window` from the hidden container.
    SetWindowData(window, it->second,
                  full_restore::FullRestoreReadHandler::GetInstance()
                      ->FetchRestoreWindowId(it->second));
    UpdateWindow(window);
    return;
  }

  // We haven't received the restore window id, add `window` to
  // `window_candidates_` to wait for the restore window id.
  window_candidates_.insert(window);
}

void LacrosReadHandler::OnWindowDestroyed(aura::Window* window) {
  window_candidates_.erase(window);
  window_to_window_data_.erase(window);
}

int32_t LacrosReadHandler::GetLacrosRestoreWindowId(
    const std::string& lacros_window_id) const {
  auto it = lacros_window_id_to_app_id_.find(lacros_window_id);
  // Set restore window id as 0 to prevent the window is added to the hidden
  // container. Windows restoration will be done by exo with another method.
  return it == lacros_window_id_to_app_id_.end()
             ? 0
             : full_restore::FullRestoreReadHandler::GetInstance()
                   ->FetchRestoreWindowId(it->second);
}

void LacrosReadHandler::SetWindowData(aura::Window* const window,
                                      const std::string& app_id,
                                      int32_t restore_window_id) {
  if (base::Contains(restore_window_id_to_app_id_, restore_window_id))
    window_to_window_data_[window].app_id = app_id;
  window_to_window_data_[window].restore_window_id = restore_window_id;
}

void LacrosReadHandler::UpdateWindow(aura::Window* const window) {
  auto it = window_to_window_data_.find(window);
  if (it != window_to_window_data_.end() && !it->second.app_id.empty()) {
    // `window` is restored, so set the window property `kRestoreWindowIdKey`
    // and `kWindowInfoKey` to restore `window`.
    window->SetProperty(kRestoreWindowIdKey, it->second.restore_window_id);
    auto window_info =
        full_restore::FullRestoreReadHandler::GetInstance()->GetWindowInfo(
            profile_path_, it->second.app_id, it->second.restore_window_id);
    if (window_info)
      ApplyProperties(window_info.get(), window);

    restore_window_id_to_app_id_.erase(it->second.restore_window_id);
  }

  // Remove the window from the hidden container.
  app_restore::AppRestoreInfo::GetInstance()->OnParentWindowToValidContainer(
      window);

  window_candidates_.erase(window);
}

}  // namespace app_restore
