// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_ANDROID_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_ANDROID_TEST_API_H_

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/android/autofill/autofill_wallet_reminder_notice_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_android.h"

namespace autofill::payments {

class ChromePaymentsAutofillClientAndroidTestApi {
 public:
  explicit ChromePaymentsAutofillClientAndroidTestApi(
      ChromePaymentsAutofillClientAndroid* client)
      : client_(CHECK_DEREF(client)) {}
  ChromePaymentsAutofillClientAndroidTestApi(
      const ChromePaymentsAutofillClientAndroidTestApi&) = delete;
  ChromePaymentsAutofillClientAndroidTestApi& operator=(
      const ChromePaymentsAutofillClientAndroidTestApi&) = delete;
  ~ChromePaymentsAutofillClientAndroidTestApi() = default;

  void SetAutofillWalletReminderNoticeBottomSheetBridge(
      std::unique_ptr<AutofillWalletReminderNoticeBottomSheetBridge> bridge) {
    client_->autofill_wallet_reminder_notice_bottom_sheet_bridge_ =
        std::move(bridge);
  }

 private:
  const raw_ref<ChromePaymentsAutofillClientAndroid> client_;
};

inline ChromePaymentsAutofillClientAndroidTestApi test_api(
    ChromePaymentsAutofillClientAndroid& client) {
  return ChromePaymentsAutofillClientAndroidTestApi(&client);
}

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_ANDROID_TEST_API_H_
