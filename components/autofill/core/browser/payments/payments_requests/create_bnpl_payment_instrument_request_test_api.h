// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_requests/create_bnpl_payment_instrument_request.h"

namespace autofill::payments {

class CreateBnplPaymentInstrumentRequestTestApi {
 public:
  explicit CreateBnplPaymentInstrumentRequestTestApi(
      CreateBnplPaymentInstrumentRequest*
          create_bnpl_payment_instrument_request)
      : create_bnpl_payment_instrument_request_(
            CHECK_DEREF(create_bnpl_payment_instrument_request)) {}
  CreateBnplPaymentInstrumentRequestTestApi(
      const CreateBnplPaymentInstrumentRequestTestApi&) = delete;
  CreateBnplPaymentInstrumentRequestTestApi& operator=(
      const CreateBnplPaymentInstrumentRequestTestApi&) = delete;
  ~CreateBnplPaymentInstrumentRequestTestApi() = default;

  std::string get_instrument_id() const {
    return create_bnpl_payment_instrument_request_->instrument_id_;
  }

 private:
  const raw_ref<CreateBnplPaymentInstrumentRequest>
      create_bnpl_payment_instrument_request_;
};

inline CreateBnplPaymentInstrumentRequestTestApi test_api(
    CreateBnplPaymentInstrumentRequest& request) {
  return CreateBnplPaymentInstrumentRequestTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
