// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service.h"

#include <cstdint>
#include <ostream>

#include "base/version.h"

namespace updater {

std::ostream& operator<<(std::ostream& os,
                         const UpdateService::UpdateState& update_state) {
  auto state_formatter = [update_state] {
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
      case UpdateService::UpdateState::State::kDecompressing:
        return "decompressing";
      case UpdateService::UpdateState::State::kPatching:
        return "patching";
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

  auto version_formatter = [update_state] {
    return base::Version(update_state.next_version).IsValid()
               ? update_state.next_version
               : "";
  };

  auto error_category_formatter = [update_state] {
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
      case UpdateService::ErrorCategory::kUnknown:
        return "unknown";
      case UpdateService::ErrorCategory::kInstaller:
        return "installer";
    }
  };

  return os << "UpdateState {app_id: " << update_state.app_id
            << ", state: " << state_formatter()
            << ", next_version: " << version_formatter()
            << ", downloaded_bytes: " << update_state.downloaded_bytes
            << ", total_bytes: " << update_state.total_bytes
            << ", install_progress: "
            << static_cast<int16_t>(update_state.install_progress)
            << ", error_category: " << error_category_formatter()
            << ", error_code: " << update_state.error_code
            << ", extra_code1: " << update_state.extra_code1 << "}";
}

bool operator==(const UpdateService::UpdateState& lhs,
                const UpdateService::UpdateState& rhs) {
  const base::Version lhs_next_version = base::Version(lhs.next_version);
  const base::Version rhs_next_version = base::Version(rhs.next_version);
  const bool versions_equal =
      (lhs_next_version.IsValid() && rhs_next_version.IsValid() &&
       lhs_next_version == rhs_next_version) ||
      (!lhs_next_version.IsValid() && !rhs_next_version.IsValid());
  return versions_equal && lhs.app_id == rhs.app_id && lhs.state == rhs.state &&
         lhs.downloaded_bytes == rhs.downloaded_bytes &&
         lhs.total_bytes == rhs.total_bytes &&
         lhs.install_progress == rhs.install_progress &&
         lhs.error_category == rhs.error_category &&
         lhs.error_code == rhs.error_code && lhs.extra_code1 == rhs.extra_code1;
}

}  // namespace updater
