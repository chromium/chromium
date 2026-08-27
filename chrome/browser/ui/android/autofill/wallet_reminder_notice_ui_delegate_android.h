// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_WALLET_REMINDER_NOTICE_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_WALLET_REMINDER_NOTICE_UI_DELEGATE_ANDROID_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"

namespace autofill {

class AutofillWalletReminderNoticeBottomSheetBridge;
class ContentAutofillClient;

namespace payments {

// Android implementation of WalletReminderNoticeUiDelegate. Triggers the Wallet
// Reminder Notice bottom sheet UI via the JNI bridge.
class WalletReminderNoticeUiDelegateAndroid
    : public WalletReminderNoticeUiDelegate {
 public:
  explicit WalletReminderNoticeUiDelegateAndroid(ContentAutofillClient* client);
  ~WalletReminderNoticeUiDelegateAndroid() override;

  WalletReminderNoticeUiDelegateAndroid(
      const WalletReminderNoticeUiDelegateAndroid&) = delete;
  WalletReminderNoticeUiDelegateAndroid& operator=(
      const WalletReminderNoticeUiDelegateAndroid&) = delete;

  // WalletReminderNoticeUiDelegate implementation:
  void ShowWalletReminderNotice(LegalMessageLines legal_message_lines) override;

  void SetAutofillWalletReminderNoticeBottomSheetBridgeForTesting(
      std::unique_ptr<AutofillWalletReminderNoticeBottomSheetBridge> bridge);

 private:
  const raw_ref<ContentAutofillClient> client_;
  std::unique_ptr<AutofillWalletReminderNoticeBottomSheetBridge>
      autofill_wallet_reminder_notice_bottom_sheet_bridge_;
};

}  // namespace payments
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_WALLET_REMINDER_NOTICE_UI_DELEGATE_ANDROID_H_
