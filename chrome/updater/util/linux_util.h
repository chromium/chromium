// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_LINUX_UTIL_H_
#define CHROME_UPDATER_UTIL_LINUX_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace updater {
enum class UpdaterScope;

// For user installations returns a path to the "~/.local" for the logged in
// user. For system installations returns "/opt/".
absl::optional<base::FilePath> GetApplicationDataDirectory(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_LINUX_UTIL_H_
