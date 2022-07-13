// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WIN_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WIN_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/common/base_platform_delegate.h"

namespace device_signals {

class WinPlatformDelegate : public BasePlatformDelegate {
 public:
  WinPlatformDelegate();
  ~WinPlatformDelegate() override;

  // PlatformDelegate:
  bool ResolveFilePath(const base::FilePath& file_path,
                       base::FilePath* resolved_file_path) override;
  ExecutableMetadata GetExecutableMetadata(
      const base::FilePath& file_path) override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WIN_PLATFORM_DELEGATE_H_
