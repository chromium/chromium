// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_WALLET_REMINDER_NOTICE_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_WALLET_REMINDER_NOTICE_REQUEST_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_requests/get_wallet_reminder_notice_request.h"

namespace autofill::payments {

class GetWalletReminderNoticeRequestTestApi {
 public:
  explicit GetWalletReminderNoticeRequestTestApi(
      GetWalletReminderNoticeRequest* get_wallet_reminder_notice_request)
      : get_wallet_reminder_notice_request_(
            CHECK_DEREF(get_wallet_reminder_notice_request)) {}
  GetWalletReminderNoticeRequestTestApi(
      const GetWalletReminderNoticeRequestTestApi&) = delete;
  GetWalletReminderNoticeRequestTestApi& operator=(
      const GetWalletReminderNoticeRequestTestApi&) = delete;
  ~GetWalletReminderNoticeRequestTestApi() = default;

  const GetWalletReminderNoticeRequestDetails& request_details() const {
    return get_wallet_reminder_notice_request_->request_details_;
  }

  const GetWalletReminderNoticeResponseDetails& response_details() const {
    return get_wallet_reminder_notice_request_->response_details_;
  }

  GetWalletReminderNoticeResponseDetails& response_details() {
    return get_wallet_reminder_notice_request_->response_details_;
  }

 private:
  const raw_ref<GetWalletReminderNoticeRequest>
      get_wallet_reminder_notice_request_;
};

inline GetWalletReminderNoticeRequestTestApi test_api(
    GetWalletReminderNoticeRequest& request) {
  return GetWalletReminderNoticeRequestTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_WALLET_REMINDER_NOTICE_REQUEST_TEST_API_H_
