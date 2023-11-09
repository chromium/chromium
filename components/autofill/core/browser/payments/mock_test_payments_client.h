// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_TEST_PAYMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_TEST_PAYMENTS_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockTestPaymentsClient : public payments::TestPaymentsClient {
 public:
  MockTestPaymentsClient();
  MockTestPaymentsClient(const MockTestPaymentsClient&) = delete;
  MockTestPaymentsClient& operator=(const MockTestPaymentsClient&) = delete;
  ~MockTestPaymentsClient() override;

  MOCK_METHOD(void,
              GetIbanUploadDetails,
              (const std::string&,
               int64_t,
               int,
               (base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                                        const std::u16string&,
                                        std::unique_ptr<base::Value::Dict>)>)),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_TEST_PAYMENTS_CLIENT_H_
