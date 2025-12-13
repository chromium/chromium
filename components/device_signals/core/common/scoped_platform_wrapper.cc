// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/scoped_platform_wrapper.h"

#include "base/files/file_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

ScopedPlatformWrapper::ScopedPlatformWrapper() {
  original_wrapper_ = PlatformWrapper::Get();
  PlatformWrapper::SetInstanceForTesting(this);

  ON_CALL(*this, PathExists(testing::_))
      .WillByDefault(
          [](const base::FilePath& path) { return base::PathExists(path); });
}

ScopedPlatformWrapper::~ScopedPlatformWrapper() {
  PlatformWrapper::SetInstanceForTesting(original_wrapper_);
}

}  // namespace device_signals
