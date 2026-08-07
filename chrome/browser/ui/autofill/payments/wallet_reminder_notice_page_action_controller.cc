// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

DEFINE_USER_DATA(WalletReminderNoticePageActionController);

WalletReminderNoticePageActionController::
    WalletReminderNoticePageActionController(
        tabs::TabInterface& tab_interface,
        page_actions::PageActionController& page_action_controller)
    : tab_interface_(tab_interface),
      page_action_controller_(page_action_controller),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

WalletReminderNoticePageActionController::
    ~WalletReminderNoticePageActionController() = default;

// static
WalletReminderNoticePageActionController*
WalletReminderNoticePageActionController::From(tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void WalletReminderNoticePageActionController::Show() {
  page_action_controller_->Show(kActionWalletReminderNotice);
}

void WalletReminderNoticePageActionController::Hide() {
  page_action_controller_->Hide(kActionWalletReminderNotice);
}

}  // namespace autofill
