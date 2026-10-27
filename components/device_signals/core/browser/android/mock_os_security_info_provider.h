// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_MOCK_OS_SECURITY_INFO_PROVIDER_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_MOCK_OS_SECURITY_INFO_PROVIDER_H_

#include "components/device_signals/core/browser/android/os_security_info_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockOsSecurityInfoProvider : public OsSecurityInfoProvider {
 public:
  MockOsSecurityInfoProvider();
  ~MockOsSecurityInfoProvider() override;

  MOCK_METHOD(bool, IsDeviceSecure, (), (override));
  MOCK_METHOD(SettingValue, GetDiskEncryption, (), (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_MOCK_OS_SECURITY_INFO_PROVIDER_H_
