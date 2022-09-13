// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/posix/posix_platform_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

PosixPlatformDelegate::PosixPlatformDelegate() = default;

PosixPlatformDelegate::~PosixPlatformDelegate() = default;

bool PosixPlatformDelegate::ResolveFilePath(
    const base::FilePath& file_path,
    base::FilePath* resolved_file_path) {
  base::FilePath local_resolved_file_path =
      base::MakeAbsoluteFilePath(file_path);
  if (local_resolved_file_path.empty()) {
    return false;
  }

  *resolved_file_path = local_resolved_file_path;
  return true;
}

}  // namespace device_signals
