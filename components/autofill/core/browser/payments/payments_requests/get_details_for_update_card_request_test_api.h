// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_CARD_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_CARD_REQUEST_TEST_API_H_

#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_card_request.h"

namespace autofill::payments {

class GetDetailsForUpdateCardRequestTestApi {
 public:
  explicit GetDetailsForUpdateCardRequestTestApi(
      GetDetailsForUpdateCardRequest* get_details_for_update_card_request)
      : get_details_for_update_card_request_(
            CHECK_DEREF(get_details_for_update_card_request)) {}
  GetDetailsForUpdateCardRequestTestApi(
      const GetDetailsForUpdateCardRequestTestApi&) = delete;
  GetDetailsForUpdateCardRequestTestApi& operator=(
      const GetDetailsForUpdateCardRequestTestApi&) = delete;
  ~GetDetailsForUpdateCardRequestTestApi() = default;

  const std::string& get_context_token() const {
    return get_details_for_update_card_request_->context_token_;
  }

 private:
  const raw_ref<GetDetailsForUpdateCardRequest>
      get_details_for_update_card_request_;
};

inline GetDetailsForUpdateCardRequestTestApi test_api(
    GetDetailsForUpdateCardRequest& request) {
  return GetDetailsForUpdateCardRequestTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_CARD_REQUEST_TEST_API_H_
