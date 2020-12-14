// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_

#include <memory>

#include "base/component_export.h"

namespace base {
class FilePath;
}

namespace full_restore {

struct AppLaunchInfo;

// FullRestoreSaveHandler is responsible for writing both the app launch
// information and the app window information to disk. FullRestoreSaveHandler
// runs on the main thread and creates FullRestoreFileHandler (which runs on a
// background task runner) for the actual writing. To minimize IO,
// FullRestoreSaveHandler starts a timer that invokes restore data saving at a
// later time.
//
// TODO(crbug.com/1146900):
// 1. Implement the restore data writing function.
// 2. Add a timer to delay the restore data saving.
class COMPONENT_EXPORT(FULL_RESTORE) FullRestoreSaveHandler {
 public:
  static FullRestoreSaveHandler* GetInstance();

  FullRestoreSaveHandler();
  virtual ~FullRestoreSaveHandler();

  FullRestoreSaveHandler(const FullRestoreSaveHandler&) = delete;
  FullRestoreSaveHandler& operator=(const FullRestoreSaveHandler&) = delete;

  void SaveAppLaunchInfo(const base::FilePath& profile_dir,
                         std::unique_ptr<AppLaunchInfo> app_launch_info);
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
