// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/system_signals/posix/posix_platform_delegate.h"

namespace device_signals {

class MacPlatformDelegate : public PosixPlatformDelegate {
 public:
  MacPlatformDelegate();
  ~MacPlatformDelegate() override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_PLATFORM_DELEGATE_H_
