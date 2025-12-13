// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_H_

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill::payments {

class MockMultipleRequestPaymentsNetworkInterface
    : public MultipleRequestPaymentsNetworkInterface {
 public:
  MockMultipleRequestPaymentsNetworkInterface(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager);
  ~MockMultipleRequestPaymentsNetworkInterface() override;

  MOCK_METHOD(
      RequestId,
      GetDetailsForCreateCard,
      (const UploadCardRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               const std::u16string&,
                               std::unique_ptr<base::Value::Dict>,
                               std::vector<std::pair<int, int>>)>),
      (override));

  MOCK_METHOD(
      RequestId,
      CreateCard,
      (const UploadCardRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               const std::string&)>),
      (override));

  MOCK_METHOD(
      RequestId,
      GetVirtualCardEnrollmentDetails,
      (const GetDetailsForEnrollmentRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               const GetDetailsForEnrollmentResponseDetails&)>),
      (override));

  MOCK_METHOD(
      RequestId,
      UpdateVirtualCardEnrollment,
      (const UpdateVirtualCardEnrollmentRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>),
      (override));
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_H_
