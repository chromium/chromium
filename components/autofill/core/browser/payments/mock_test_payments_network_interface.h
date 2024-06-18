// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_TEST_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_TEST_PAYMENTS_NETWORK_INTERFACE_H_

#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockTestPaymentsNetworkInterface : public payments::TestPaymentsNetworkInterface {
 public:
  MockTestPaymentsNetworkInterface();
  MockTestPaymentsNetworkInterface(const MockTestPaymentsNetworkInterface&) = delete;
  MockTestPaymentsNetworkInterface& operator=(const MockTestPaymentsNetworkInterface&) = delete;
  ~MockTestPaymentsNetworkInterface() override;

  MOCK_METHOD(void,
              GetIbanUploadDetails,
              (const std::string&,
               int64_t,
               int,
               const std::string&,
               (base::OnceCallback<
                   void(payments::PaymentsAutofillClient::PaymentsRpcResult,
                        const std::u16string&,
                        const std::u16string&,
                        std::unique_ptr<base::Value::Dict>)>)),
              (override));
  MOCK_METHOD(
      void,
      UnmaskIban,
      (const payments::PaymentsNetworkInterface::UnmaskIbanRequestDetails&,
       (base::OnceCallback<
           void(payments::PaymentsAutofillClient::PaymentsRpcResult,
                const std::u16string&)>)),
      (override));
  MOCK_METHOD(
      void,
      UploadIban,
      (const UploadIbanRequestDetails&,
       (base::OnceCallback<void(
            payments::PaymentsAutofillClient::PaymentsRpcResult)> callback)),
      (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_TEST_PAYMENTS_NETWORK_INTERFACE_H_
