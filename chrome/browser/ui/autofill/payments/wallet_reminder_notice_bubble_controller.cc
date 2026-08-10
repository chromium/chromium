// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"

#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

DEFINE_USER_DATA(WalletReminderNoticeBubbleController);

WalletReminderNoticeBubbleController::WalletReminderNoticeBubbleController(
    tabs::TabInterface& tab_interface,
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      tab_interface_(tab_interface),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

WalletReminderNoticeBubbleController::~WalletReminderNoticeBubbleController() =
    default;

// static
WalletReminderNoticeBubbleController*
WalletReminderNoticeBubbleController::From(tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

AutofillBubbleBase* WalletReminderNoticeBubbleController::GetBubbleView()
    const {
  return bubble_view();
}

base::WeakPtr<WalletReminderNoticeBubbleController>
WalletReminderNoticeBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BubbleType WalletReminderNoticeBubbleController::GetBubbleType() const {
  return BubbleType::kWalletReminderNotice;
}

base::WeakPtr<BubbleControllerBase>
WalletReminderNoticeBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WalletReminderNoticeBubbleController::DoShowBubble() {
  // TODO(crbug.com/543546376): Show Wallet reminder notice bubble.
}

}  // namespace autofill
