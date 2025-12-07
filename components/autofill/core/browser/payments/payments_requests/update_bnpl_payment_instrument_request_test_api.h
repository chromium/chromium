// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_requests/update_bnpl_payment_instrument_request.h"

namespace autofill::payments {

class UpdateBnplPaymentInstrumentRequestTestApi {
 public:
  explicit UpdateBnplPaymentInstrumentRequestTestApi(
      UpdateBnplPaymentInstrumentRequest*
          update_bnpl_payment_instrument_request)
      : update_bnpl_payment_instrument_request_(
            CHECK_DEREF(update_bnpl_payment_instrument_request)) {}
  UpdateBnplPaymentInstrumentRequestTestApi(
      const UpdateBnplPaymentInstrumentRequestTestApi&) = delete;
  UpdateBnplPaymentInstrumentRequestTestApi& operator=(
      const UpdateBnplPaymentInstrumentRequestTestApi&) = delete;
  ~UpdateBnplPaymentInstrumentRequestTestApi() = default;

 private:
  const raw_ref<UpdateBnplPaymentInstrumentRequest>
      update_bnpl_payment_instrument_request_;
};

inline UpdateBnplPaymentInstrumentRequestTestApi test_api(
    UpdateBnplPaymentInstrumentRequest& request) {
  return UpdateBnplPaymentInstrumentRequestTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
