// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_CARD_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_CARD_REQUEST_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_card_request.h"

namespace autofill::payments {

class GetDetailsForCreateCardTestApi {
 public:
  explicit GetDetailsForCreateCardTestApi(
      GetDetailsForCreateCardRequest* request)
      : request_(CHECK_DEREF(request)) {}
  GetDetailsForCreateCardTestApi(const GetDetailsForCreateCardTestApi&) =
      delete;
  GetDetailsForCreateCardTestApi& operator=(
      const GetDetailsForCreateCardTestApi&) = delete;
  ~GetDetailsForCreateCardTestApi() = default;

  std::u16string context_token() const { return request_->context_token_; }

  base::Value::Dict* legal_message() const {
    return request_->legal_message_.get();
  }

  std::vector<std::pair<int, int>> supported_card_bin_ranges() const {
    return request_->supported_card_bin_ranges_;
  }

 private:
  const raw_ref<GetDetailsForCreateCardRequest> request_;
};

inline GetDetailsForCreateCardTestApi test_api(
    GetDetailsForCreateCardRequest& request) {
  return GetDetailsForCreateCardTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_CARD_REQUEST_TEST_API_H_
