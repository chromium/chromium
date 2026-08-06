// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class WalletReminderNoticePageActionController {
 public:
  DECLARE_USER_DATA(WalletReminderNoticePageActionController);

  WalletReminderNoticePageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~WalletReminderNoticePageActionController();

  WalletReminderNoticePageActionController(
      const WalletReminderNoticePageActionController&) = delete;
  WalletReminderNoticePageActionController& operator=(
      const WalletReminderNoticePageActionController&) = delete;

  static WalletReminderNoticePageActionController* From(
      tabs::TabInterface& tab);

  // Shows the wallet reminder notice page action.
  void Show();

  // Hides the wallet reminder notice page action.
  void Hide();

 private:
  const raw_ref<tabs::TabInterface> tab_interface_;

  const raw_ref<page_actions::PageActionController> page_action_controller_;

  ui::ScopedUnownedUserData<WalletReminderNoticePageActionController>
      scoped_unowned_user_data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_PAGE_ACTION_CONTROLLER_H_
