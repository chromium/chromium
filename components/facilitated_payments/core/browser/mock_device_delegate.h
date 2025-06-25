// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_DEVICE_DELEGATE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_DEVICE_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {

class MockDeviceDelegate : public DeviceDelegate {
 public:
  MockDeviceDelegate();
  ~MockDeviceDelegate() override;

  MOCK_METHOD(bool, IsPixAccountLinkingSupported, (), (const, override));
  MOCK_METHOD(void, LaunchPixAccountLinkingPage, (), (override));
  MOCK_METHOD(void,
              SetOnReturnToChromeCallback,
              (base::OnceClosure),
              (override));
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_DEVICE_DELEGATE_H_
