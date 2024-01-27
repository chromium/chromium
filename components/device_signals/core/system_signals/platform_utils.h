// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_UTILS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_UTILS_H_

#include <optional>

#include "base/process/process_handle.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

// Returns the file path pointing to the executable file that spawned
// the given process `pid`.
std::optional<base::FilePath> GetProcessExePath(base::ProcessId pid);

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_UTILS_H_
