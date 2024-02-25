// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_PLATFORM_UTILS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_PLATFORM_UTILS_H_

#include <optional>

#include "base/process/process_handle.h"
#include "build/build_config.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

struct CrowdStrikeSignals;

// Extracts the common details for resolving a file path on different
// platforms. Resolves environment variables and relative markers in
// `file_path`, and returns the path via `resolved_file_path`. For
// consistency on all platforms, this method will return false if no file
// system item resides at the end path and true otherwise.
bool ResolvePath(const base::FilePath& file_path,
                 base::FilePath* resolved_file_path);

// Returns the file path pointing to the executable file that spawned
// the given process `pid`.
std::optional<base::FilePath> GetProcessExePath(base::ProcessId pid);

// Returns details about an installed CrowdStrike agent (if any) read
// from location which can be accessed synchronously (i.e. not the
// data.zta file). For a more robust retrieval, see the
// CrowdStrikeClient class.
std::optional<CrowdStrikeSignals> GetCrowdStrikeSignals();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
base::FilePath GetCrowdStrikeZtaFilePath();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_PLATFORM_UTILS_H_
