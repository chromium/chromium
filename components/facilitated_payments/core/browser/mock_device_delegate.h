// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_DEVICE_DELEGATE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_DEVICE_DELEGATE_H_

#include <string_view>

#include "base/functional/callback.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {

class MockDeviceDelegate : public DeviceDelegate {
 public:
  MockDeviceDelegate();
  ~MockDeviceDelegate() override;

  MOCK_METHOD(WalletEligibilityForPixAccountLinking,
              IsPixAccountLinkingSupported,
              (),
              (const, override));
  MOCK_METHOD(void, LaunchPixAccountLinkingPage, (std::string), (override));
  MOCK_METHOD(void,
              SetOnReturnToChromeCallbackAndObserveAppState,
              (base::OnceClosure),
              (override));
  MOCK_METHOD(std::unique_ptr<FacilitatedPaymentsAppInfoList>,
              GetSupportedPaymentApps,
              (const GURL& payment_link_url),
              (override));
  MOCK_METHOD(bool,
              InvokePaymentApp,
              (std::string_view package_name,
               std::string_view activity_name,
               const GURL& payment_link_url),
              (override));
  MOCK_METHOD(bool, IsPixSupportAvailableViaGboard, (), (const, override));
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_DEVICE_DELEGATE_H_
