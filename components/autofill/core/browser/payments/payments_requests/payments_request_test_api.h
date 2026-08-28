// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_TEST_API_H_

#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class PaymentsRequestTestApi {
 public:
  explicit PaymentsRequestTestApi(PaymentsRequest* payments_request)
      : payments_request_(payments_request) {}
  PaymentsRequestTestApi(const PaymentsRequestTestApi&) = delete;
  PaymentsRequestTestApi& operator=(const PaymentsRequestTestApi&) = delete;
  ~PaymentsRequestTestApi() = default;

  base::DictValue BuildChromeUserContext() const {
    return payments_request_->BuildChromeUserContext();
  }

  PaymentsRequest::ClientType GetChromeUserContextClientType() const {
    return payments_request_->GetChromeUserContextClientType();
  }

 private:
  const raw_ptr<PaymentsRequest> payments_request_;
};

inline PaymentsRequestTestApi test_api(PaymentsRequest* request) {
  return PaymentsRequestTestApi(request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_TEST_API_H_
