// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_desktop.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"
#include "components/tabs/public/tab_interface.h"

namespace autofill::payments {

// static
std::unique_ptr<ChromePaymentsAutofillClient>
ChromePaymentsAutofillClient::Create(ContentAutofillClient* client) {
  return std::make_unique<ChromePaymentsAutofillClientDesktop>(client);
}

ChromePaymentsAutofillClientDesktop::ChromePaymentsAutofillClientDesktop(
    ContentAutofillClient* client)
    : ChromePaymentsAutofillClient(client) {}

ChromePaymentsAutofillClientDesktop::~ChromePaymentsAutofillClientDesktop() =
    default;

void ChromePaymentsAutofillClientDesktop::ShowWalletReminderNotice(
    LegalMessageLines legal_message_lines) {
  if (!web_contents()) {
    return;
  }
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
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
