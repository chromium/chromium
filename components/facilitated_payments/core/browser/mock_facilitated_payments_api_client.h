// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_API_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_API_CLIENT_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {

// A mock for the facilitated payment API client interface.
class MockFacilitatedPaymentsApiClient : public FacilitatedPaymentsApiClient {
 public:
  static std::unique_ptr<FacilitatedPaymentsApiClient> CreateApiClient();

  MockFacilitatedPaymentsApiClient();
  ~MockFacilitatedPaymentsApiClient() override;

  MOCK_METHOD(void, IsAvailable, (base::OnceCallback<void(bool)>), (override));
  MOCK_METHOD(void,
              GetClientToken,
              (base::OnceCallback<void(std::vector<uint8_t>)>),
              (override));
  MOCK_METHOD(void,
              InvokePurchaseAction,
              (CoreAccountInfo,
               base::span<const uint8_t>,
               base::OnceCallback<void(PurchaseActionResult)>),
              (override));
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_API_CLIENT_H_
