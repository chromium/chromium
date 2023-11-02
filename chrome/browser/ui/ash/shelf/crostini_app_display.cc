// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/crostini_app_display.h"

#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

CrostiniAppDisplay::CrostiniAppDisplay() = default;

CrostiniAppDisplay::~CrostiniAppDisplay() = default;

void CrostiniAppDisplay::Register(const std::string& app_id,
                                  int64_t display_id) {
  while (app_ids_.size() >= kMaxAppIdSize) {
    app_id_to_display_id_.erase(app_ids_.front());
    app_ids_.pop_front();
  }
  auto it = app_id_to_display_id_.find(app_id);
  if (it == app_id_to_display_id_.end()) {
    app_id_to_display_id_.emplace(app_id, display_id);
    app_ids_.push_back(app_id);
  } else {
    it->second = display_id;
  }
}

int64_t CrostiniAppDisplay::GetDisplayIdForAppId(const std::string& app_id) {
  auto it = app_id_to_display_id_.find(app_id);
  if (it == app_id_to_display_id_.end())
    return display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  return it->second;
}
