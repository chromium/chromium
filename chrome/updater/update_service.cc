// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service.h"

#include <ostream>

namespace updater {

UpdateService::UpdateState::UpdateState() = default;
UpdateService::UpdateState::UpdateState(const UpdateState&) = default;
UpdateService::UpdateState& UpdateService::UpdateState::operator=(
    const UpdateState&) = default;
UpdateService::UpdateState::UpdateState(UpdateState&&) = default;
UpdateService::UpdateState& UpdateService::UpdateState::operator=(
    UpdateState&&) = default;
UpdateService::UpdateState::~UpdateState() = default;

std::ostream& operator<<(std::ostream& os,
                         const UpdateService::UpdateState& update_state) {
  auto state_formatter = [update_state]() {
    switch (update_state.state) {
      case UpdateService::UpdateState::State::kUnknown:
        return "unknown";
      case UpdateService::UpdateState::State::kNotStarted:
        return "not started";
      case UpdateService::UpdateState::State::kCheckingForUpdates:
        return "checking for updates";
      case UpdateService::UpdateState::State::kUpdateAvailable:
        return "update available";
      case UpdateService::UpdateState::State::kDownloading:
        return "downloading";
      case UpdateService::UpdateState::State::kInstalling:
        return "installing";
      case UpdateService::UpdateState::State::kUpdated:
        return "updated";
      case UpdateService::UpdateState::State::kNoUpdate:
        return "no update";
      case UpdateService::UpdateState::State::kUpdateError:
        return "update error";
    }
  };

  auto version_formatter = [update_state]() {
    return update_state.next_version.IsValid()
               ? update_state.next_version.GetString()
               : "";
  };

  auto error_category_formatter = [update_state]() {
    switch (update_state.error_category) {
      case UpdateService::ErrorCategory::kNone:
        return "none";
      case UpdateService::ErrorCategory::kDownload:
        return "download";
      case UpdateService::ErrorCategory::kUnpack:
        return "unpack";
      case UpdateService::ErrorCategory::kInstall:
        return "install";
      case UpdateService::ErrorCategory::kService:
        return "service";
      case UpdateService::ErrorCategory::kUpdateCheck:
        return "update check";
    }
  };

  return os << "UpdateState {app_id: " << update_state.app_id
            << ", state: " << state_formatter()
            << ", next_version: " << version_formatter()
            << ", downloaded_bytes: " << update_state.downloaded_bytes
            << ", total_bytes: " << update_state.total_bytes
            << ", install_progress: " << update_state.install_progress
            << ", error_category: " << error_category_formatter()
            << ", error_code: " << update_state.error_code
            << ", extra_code1: " << update_state.extra_code1 << "}";
}

}  // namespace updater
