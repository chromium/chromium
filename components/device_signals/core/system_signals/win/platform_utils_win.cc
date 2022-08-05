// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/platform_utils.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/process/process.h"

namespace device_signals {

absl::optional<base::FilePath> GetProcessExePath(base::ProcessId pid) {
  base::Process process(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.IsValid()) {
    return absl::nullopt;
  }

  DWORD path_len = MAX_PATH;
  wchar_t path_string[MAX_PATH];
  if (!::QueryFullProcessImageName(process.Handle(), 0, path_string,
                                   &path_len)) {
    return absl::nullopt;
  }

  return base::FilePath(path_string);
}

}  // namespace device_signals
