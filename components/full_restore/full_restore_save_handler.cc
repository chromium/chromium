// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_save_handler.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/full_restore/app_launch_info.h"

namespace full_restore {

FullRestoreSaveHandler* FullRestoreSaveHandler::GetInstance() {
  static base::NoDestructor<FullRestoreSaveHandler> full_restore_save_handler;
  return full_restore_save_handler.get();
}

FullRestoreSaveHandler::FullRestoreSaveHandler() = default;

FullRestoreSaveHandler::~FullRestoreSaveHandler() = default;

void FullRestoreSaveHandler::SaveAppLaunchInfo(
    const base::FilePath& profile_dir,
    std::unique_ptr<AppLaunchInfo> app_launch_info) {
  // TODO(crbug.com/1146900): Save the app launch parameters to the full restore
  // file.
}

}  // namespace full_restore
