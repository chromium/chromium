// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_save_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/restore_data.h"

namespace full_restore {

namespace {

// Delay between when an update is received, and when we save it to the
// full restore file.
constexpr base::TimeDelta kSaveDelay = base::TimeDelta::FromMilliseconds(2500);

}  // namespace

FullRestoreSaveHandler* FullRestoreSaveHandler::GetInstance() {
  static base::NoDestructor<FullRestoreSaveHandler> full_restore_save_handler;
  return full_restore_save_handler.get();
}

FullRestoreSaveHandler::FullRestoreSaveHandler() = default;

FullRestoreSaveHandler::~FullRestoreSaveHandler() = default;

void FullRestoreSaveHandler::SaveAppLaunchInfo(
    const base::FilePath& file_path,
    std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  // Each user should have one full restore file saving the restore data in the
  // profile directory |file_path|. So |app_launch_info| is saved to the restore
  // data for the user with the profile path |file_path|.
  file_path_to_restore_data_[file_path].AddAppLaunchInfo(
      std::move(app_launch_info));

  should_update_.insert(file_path);

  MaybeStartSaveTimer();
}

void FullRestoreSaveHandler::MaybeStartSaveTimer() {
  if (!save_timer_.IsRunning() && save_running_.empty()) {
    save_timer_.Start(FROM_HERE, kSaveDelay,
                      base::BindOnce(&FullRestoreSaveHandler::Save,
                                     weak_factory_.GetWeakPtr()));
  }
}

void FullRestoreSaveHandler::Save() {
  if (should_update_.empty())
    return;

  for (const auto& file_path : should_update_) {
    save_running_.insert(file_path);
    BackendTaskRunner(file_path)->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&FullRestoreFileHandler::WriteToFile,
                       GetFileHandler(file_path),
                       file_path_to_restore_data_[file_path].Clone()),
        base::BindOnce(&FullRestoreSaveHandler::OnSaveFinished,
                       weak_factory_.GetWeakPtr(), file_path));
  }
  should_update_.clear();
}

void FullRestoreSaveHandler::OnSaveFinished(const base::FilePath& file_path) {
  save_running_.erase(file_path);
}

FullRestoreFileHandler* FullRestoreSaveHandler::GetFileHandler(
    const base::FilePath& file_path) {
  if (file_path_to_file_handler_.find(file_path) ==
      file_path_to_file_handler_.end()) {
    file_path_to_file_handler_[file_path] =
        base::MakeRefCounted<FullRestoreFileHandler>(file_path);
  }
  return file_path_to_file_handler_[file_path].get();
}

base::SequencedTaskRunner* FullRestoreSaveHandler::BackendTaskRunner(
    const base::FilePath& file_path) {
  return GetFileHandler(file_path)->owning_task_runner();
}

}  // namespace full_restore
