// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_UI_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_UI_DELEGATE_DESKTOP_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"

namespace autofill {
class ContentAutofillClient;

namespace payments {

// Desktop implementation of the WalletReminderNoticeUiDelegate interface.
// This class handles the UI for the Wallet reminder notice on the Desktop
// platform.
class WalletReminderNoticeUiDelegateDesktop
    : public WalletReminderNoticeUiDelegate {
 public:
  explicit WalletReminderNoticeUiDelegateDesktop(ContentAutofillClient* client);
  WalletReminderNoticeUiDelegateDesktop(
      const WalletReminderNoticeUiDelegateDesktop& other) = delete;
  WalletReminderNoticeUiDelegateDesktop& operator=(
      const WalletReminderNoticeUiDelegateDesktop& other) = delete;
  ~WalletReminderNoticeUiDelegateDesktop() override;

  // WalletReminderNoticeUiDelegate:
  void ShowWalletReminderNotice(LegalMessageLines legal_message_lines) override;

 private:
  const raw_ref<ContentAutofillClient> client_;
};

}  // namespace payments
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_UI_DELEGATE_DESKTOP_H_
