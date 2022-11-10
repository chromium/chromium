// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_
#define CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_

#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace updater {

absl::optional<base::FilePath> GetActiveDutySocketPath(UpdaterScope scope);

absl::optional<base::FilePath> GetActiveDutyInternalSocketPath(
    UpdaterScope scope,
    const base::Version& version);

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_
