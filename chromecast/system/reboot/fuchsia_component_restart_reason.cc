// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/fuchsia_component_restart_reason.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chromecast/public/reboot_shlib.h"

namespace chromecast {
namespace {
constexpr char kStartedOnce[] = "component-started-once";
constexpr char kGracefulTeardown[] = "component-graceful-teardown";
constexpr char kSubFolder[] = "lifecycle";

void CreateFlagFile(const base::FilePath& file) {
  if (!base::WriteFile(file, "")){
    LOG(ERROR) << "Cannot create file " << file
               << ", will not correctly determine restart reason.";
  }
}

}  // namespace

FuchsiaComponentRestartReason::FuchsiaComponentRestartReason() {
  if (!base::GetTempDir(&tmp_dir_)){
    LOG(ERROR) << "tmp file dir cannot be obtained.";
  }
  tmp_dir_ = tmp_dir_.Append(kSubFolder);
  base::CreateDirectory(tmp_dir_);
}

// Return True if it was restart instead of reboot
bool FuchsiaComponentRestartReason::GetRestartReason(
    RebootShlib::RebootSource* restart_reason) {
  if (!restart_checked_) {
    restart_checked_ = true;
    base::FilePath path_started_once = tmp_dir_.Append(kStartedOnce);
    base::FilePath path_graceful_teardown = tmp_dir_.Append(kGracefulTeardown);
    if (base::PathExists(path_graceful_teardown)) {
      // We come out of graceful restart
      restart_reason_ = RebootShlib::RebootSource::GRACEFUL_RESTART;
      if (!base::DeleteFile(path_graceful_teardown)){
        LOG(ERROR) << "Cannot delete file " << path_graceful_teardown
                   << ", will not correctly determine restart reason.";
      }
    } else if (base::PathExists(path_started_once)){
      // We come out of ungraceful restart
      restart_reason_ = RebootShlib::RebootSource::UNGRACEFUL_RESTART;
    } else {
      was_restart_ = false;
    }

    // The file path is inside /tmp which is guaranteed to be removed after
    // reboot to distinguish reboot from restart.
    CreateFlagFile(path_started_once);
  }
  if (was_restart_)
    *restart_reason = restart_reason_;

  return was_restart_;
}

void FuchsiaComponentRestartReason::ResetRestartCheck() {
  restart_checked_ = false;
  was_restart_ = true;
}

const base::FilePath& FuchsiaComponentRestartReason::SetFlagFileDirForTesting(
    const base::FilePath& path) {
  tmp_dir_ = path.Append(kSubFolder);
  base::CreateDirectory(tmp_dir_);
  return tmp_dir_;
}

void FuchsiaComponentRestartReason::RegisterTeardown() {
  CreateFlagFile(tmp_dir_.Append(kGracefulTeardown));
}

}  // namespace chromecast
