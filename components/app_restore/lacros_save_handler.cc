// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/lacros_save_handler.h"

#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_properties.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"

namespace full_restore {

LacrosSaveHandler::LacrosSaveHandler(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

LacrosSaveHandler::~LacrosSaveHandler() = default;

void LacrosSaveHandler::OnWindowInitialized(aura::Window* window) {
  const std::string* lacros_window_id =
      window->GetProperty(app_restore::kLacrosWindowId);
  DCHECK(lacros_window_id);
  window_candidates_[*lacros_window_id].app_id = extension_misc::kLacrosAppId;
  window_candidates_[*lacros_window_id].window_id = ++window_id_;

  auto app_launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      extension_misc::kLacrosAppId, window_id_);
  FullRestoreSaveHandler::GetInstance()->AddAppLaunchInfo(
      profile_path_, std::move(app_launch_info));
}

void LacrosSaveHandler::OnWindowDestroyed(aura::Window* window) {
  const std::string* lacros_window_id =
      window->GetProperty(app_restore::kLacrosWindowId);
  DCHECK(lacros_window_id);
  auto it = window_candidates_.find(*lacros_window_id);
  if (it == window_candidates_.end())
    return;

  FullRestoreSaveHandler::GetInstance()->RemoveAppRestoreData(
      profile_path_, it->second.app_id, it->second.window_id);

  window_candidates_.erase(it);
}

}  // namespace full_restore
