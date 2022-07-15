// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/base_platform_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

BasePlatformDelegate::BasePlatformDelegate() = default;
BasePlatformDelegate::~BasePlatformDelegate() = default;

bool BasePlatformDelegate::PathIsReadable(
    const base::FilePath& file_path) const {
  return base::PathIsReadable(file_path);
}

bool BasePlatformDelegate::DirectoryExists(
    const base::FilePath& file_path) const {
  return base::DirectoryExists(file_path);
}

}  // namespace device_signals
