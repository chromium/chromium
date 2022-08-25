// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/executable_metadata_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "build/build_config.h"

namespace device_signals {

ExecutableMetadataService::ExecutableMetadataService(
    std::unique_ptr<PlatformDelegate> platform_delegate)
    : platform_delegate_(std::move(platform_delegate)) {
  DCHECK(platform_delegate_);
}

ExecutableMetadataService::~ExecutableMetadataService() = default;

}  // namespace device_signals
