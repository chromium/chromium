// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_ANDROID_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_ANDROID_H_

#include <memory>

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {

class AutofillWalletReminderNoticeBottomSheetBridge;
class ContentAutofillClient;

namespace payments {

// Android implementation of PaymentsAutofillClient for Chrome (Clank). Owned
// by ChromeAutofillClient.
class ChromePaymentsAutofillClientAndroid
    : public ChromePaymentsAutofillClient {
 public:
  explicit ChromePaymentsAutofillClientAndroid(ContentAutofillClient* client);
  ChromePaymentsAutofillClientAndroid(
      const ChromePaymentsAutofillClientAndroid&) = delete;
  ChromePaymentsAutofillClientAndroid& operator=(
      const ChromePaymentsAutofillClientAndroid&) = delete;
  ~ChromePaymentsAutofillClientAndroid() override;

  // PaymentsAutofillClient:
  void ShowWalletReminderNotice(LegalMessageLines legal_message_lines) final;

 private:
  friend class ChromePaymentsAutofillClientAndroidTestApi;
  AutofillWalletReminderNoticeBottomSheetBridge*
  GetOrCreateWalletReminderNoticeBridge();

  std::unique_ptr<AutofillWalletReminderNoticeBottomSheetBridge>
      autofill_wallet_reminder_notice_bottom_sheet_bridge_;
};

}  // namespace payments
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_ANDROID_H_
