// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_ui_delegate_desktop.h"

namespace autofill::payments {

WalletReminderNoticeUiDelegateDesktop::WalletReminderNoticeUiDelegateDesktop() =
    default;

WalletReminderNoticeUiDelegateDesktop::
    ~WalletReminderNoticeUiDelegateDesktop() = default;

void WalletReminderNoticeUiDelegateDesktop::ShowWalletReminderNotice(
    LegalMessageLines legal_message_lines) {
  // TODO(crbug.com/545685236): Implement ShowWalletReminderNotice which will
  // display the page action icon and bubble.
}

}  // namespace autofill::payments
