// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_utils.h"

#include "ash/public/cpp/ash_features.h"
#include "base/files/file_path.h"
#include "components/full_restore/app_launch_info.h"

namespace {

// Set the default restore flag as false, and it can be reset based on the
// restore setting and the user's choice from notifications.
bool g_restore = false;

}  // namespace

namespace full_restore {

void SaveAppLaunchInfo(const base::FilePath& profile_dir,
                       std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!ash::features::IsFullRestoreEnabled())
    return;

  // TODO(crbug.com/1146900): Save the app launch parameters to the full restore
  // file.
}

bool ShouldRestore() {
  return g_restore;
}

void SetRestoreFlag(bool should_restore) {
  g_restore = should_restore;
}

}  // namespace full_restore
