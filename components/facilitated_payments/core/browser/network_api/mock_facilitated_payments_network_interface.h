// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_MOCK_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_MOCK_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_

#include <memory>

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {

class MockFacilitatedPaymentsNetworkInterface
    : public FacilitatedPaymentsNetworkInterface {
 public:
  MockFacilitatedPaymentsNetworkInterface(
      signin::IdentityManager& identity_manager,
      autofill::AccountInfoGetter& account_info_getter);
  ~MockFacilitatedPaymentsNetworkInterface() override;

  MOCK_METHOD(
      RequestId,
      InitiatePayment,
      (std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>,
       InitiatePaymentResponseCallback,
       const std::string&),
      (override));
  MOCK_METHOD(RequestId,
              GetDetailsForCreatePaymentInstrument,
              (int64_t,
               GetDetailsForCreatePaymentInstrumentResponseCallback,
               const std::string&),
              (override));
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_MOCK_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_
