// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_POSIX_POSIX_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_POSIX_POSIX_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/system_signals/base_platform_delegate.h"

namespace device_signals {

class PosixPlatformDelegate : public BasePlatformDelegate {
 public:
  PosixPlatformDelegate();
  ~PosixPlatformDelegate() override;

  // PlatformDelegate:
  bool ResolveFilePath(const base::FilePath& file_path,
                       base::FilePath* resolved_file_path) override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_POSIX_POSIX_PLATFORM_DELEGATE_H_
