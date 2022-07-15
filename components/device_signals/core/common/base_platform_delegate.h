// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_BASE_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_BASE_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/common/platform_delegate.h"

namespace device_signals {

// Implements some functionality that is common to all PlatformDelegate
// specializations.
class BasePlatformDelegate : public PlatformDelegate {
 public:
  ~BasePlatformDelegate() override;

  // PlatformDelegate:
  bool PathIsReadable(const base::FilePath& file_path) const override;
  bool DirectoryExists(const base::FilePath& file_path) const override;

 protected:
  BasePlatformDelegate();
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_BASE_PLATFORM_DELEGATE_H_
