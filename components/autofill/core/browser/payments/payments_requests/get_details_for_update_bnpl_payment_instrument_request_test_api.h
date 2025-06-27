// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_bnpl_payment_instrument_request.h"

namespace autofill::payments {

class GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi {
 public:
  explicit GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi(
      GetDetailsForUpdateBnplPaymentInstrumentRequest*
          get_details_for_update_bnpl_payment_instrument_request)
      : get_details_for_update_bnpl_payment_instrument_request_(CHECK_DEREF(
            get_details_for_update_bnpl_payment_instrument_request)) {}
  GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi(
      const GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi&) = delete;
  GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi& operator=(
      const GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi&) = delete;
  ~GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi() = default;

  std::string get_context_token() const {
    return get_details_for_update_bnpl_payment_instrument_request_
        ->context_token_;
  }

  LegalMessageLines& get_legal_message() const {
    return get_details_for_update_bnpl_payment_instrument_request_
        ->legal_message_;
  }

 private:
  const raw_ref<GetDetailsForUpdateBnplPaymentInstrumentRequest>
      get_details_for_update_bnpl_payment_instrument_request_;
};

inline GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi test_api(
    GetDetailsForUpdateBnplPaymentInstrumentRequest& request) {
  return GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
