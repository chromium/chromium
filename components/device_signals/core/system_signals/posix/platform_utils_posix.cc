// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/platform_utils.h"

#include "base/files/file_path.h"
#include "base/process/process_handle.h"

namespace device_signals {

absl::optional<base::FilePath> GetProcessExePath(base::ProcessId pid) {
  auto file_path = base::GetProcessExecutablePath(pid);
  if (file_path.empty()) {
    return absl::nullopt;
  }
  return file_path;
}

}  // namespace device_signals
