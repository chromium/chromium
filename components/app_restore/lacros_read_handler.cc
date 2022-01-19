// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/lacros_read_handler.h"

#include "ash/constants/app_types.h"
#include "components/app_restore/app_restore_utils.h"
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
    uint32_t restored_browser_session_id) {
  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::LACROS)) {
    lacros_window_id_to_restore_window_id_[app_restore::GetLacrosWindowId(
        window)] = restored_browser_session_id;
  }
}

}  // namespace app_restore
