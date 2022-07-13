// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MAC_MAC_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MAC_MAC_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/common/posix/posix_platform_delegate.h"

namespace device_signals {

class MacPlatformDelegate : public PosixPlatformDelegate {
 public:
  MacPlatformDelegate();
  ~MacPlatformDelegate() override;

  // FileSystemDelegate:
  ExecutableMetadata GetExecutableMetadata(
      const base::FilePath& file_path) override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MAC_MAC_PLATFORM_DELEGATE_H_
