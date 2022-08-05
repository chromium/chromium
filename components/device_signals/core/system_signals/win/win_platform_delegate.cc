// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/win_platform_delegate.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "components/device_signals/core/common/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {

namespace {

// Helper function for expanding all environment variables in `path`.
absl::optional<std::wstring> ExpandEnvironmentVariables(
    const std::wstring& path) {
  static const DWORD kMaxBuffer = 32 * 1024;  // Max according to MSDN.
  std::wstring path_expanded;
  DWORD path_len = MAX_PATH;
  do {
    DWORD result = ::ExpandEnvironmentStrings(
        path.c_str(), base::WriteInto(&path_expanded, path_len), path_len);
    if (!result) {
      // Failed to expand variables.
      break;
    }
    if (result <= path_len)
      return path_expanded.substr(0, result - 1);
    path_len = result;
  } while (path_len < kMaxBuffer);

  return absl::nullopt;
}

}  // namespace

WinPlatformDelegate::WinPlatformDelegate() = default;

WinPlatformDelegate::~WinPlatformDelegate() = default;

bool WinPlatformDelegate::ResolveFilePath(const base::FilePath& file_path,
                                          base::FilePath* resolved_file_path) {
  auto expanded_path_wstring = ExpandEnvironmentVariables(file_path.value());
  if (!expanded_path_wstring) {
    return false;
  }

  auto expanded_file_path = base::FilePath(expanded_path_wstring.value());
  if (!base::PathExists(expanded_file_path)) {
    return false;
  }

  *resolved_file_path = base::MakeAbsoluteFilePath(expanded_file_path);
  return true;
}

}  // namespace device_signals
