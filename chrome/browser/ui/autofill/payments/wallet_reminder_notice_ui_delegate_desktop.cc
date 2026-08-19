// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_ui_delegate_desktop.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/tabs/public/tab_interface.h"

namespace autofill::payments {

WalletReminderNoticeUiDelegateDesktop::WalletReminderNoticeUiDelegateDesktop(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

WalletReminderNoticeUiDelegateDesktop::
    ~WalletReminderNoticeUiDelegateDesktop() = default;

void WalletReminderNoticeUiDelegateDesktop::ShowWalletReminderNotice(
    LegalMessageLines legal_message_lines) {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&client_->GetWebContents());
  if (!tab_interface) {
    return;
  }

  if (WalletReminderNoticePageActionController* page_action_controller =
          WalletReminderNoticePageActionController::From(*tab_interface)) {
    page_action_controller->Show();
  }

  if (WalletReminderNoticeBubbleController* bubble_controller =
          WalletReminderNoticeBubbleController::From(*tab_interface)) {
    bubble_controller->Show(std::move(legal_message_lines));
  }
}

}  // namespace autofill::payments
