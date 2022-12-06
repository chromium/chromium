// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_
#define CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_

#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace updater {

base::FilePath GetActiveDutySocketPath(UpdaterScope scope);

base::FilePath GetActiveDutyInternalSocketPath(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_
