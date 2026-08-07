// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_WALLET_REMINDER_NOTICE_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_WALLET_REMINDER_NOTICE_UI_DELEGATE_H_

#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {
namespace payments {

// The cross-platform C++ UI delegate interface for displaying the Wallet
// Reminder Notice UI.
class WalletReminderNoticeUiDelegate {
 public:
  virtual ~WalletReminderNoticeUiDelegate() = default;

  // Requests the platform UI layer to show the Wallet Reminder Notice bottom
  // sheet/dialog displaying `legal_message_lines`.
  virtual void ShowWalletReminderNotice(
      LegalMessageLines legal_message_lines) = 0;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_WALLET_REMINDER_NOTICE_UI_DELEGATE_H_
