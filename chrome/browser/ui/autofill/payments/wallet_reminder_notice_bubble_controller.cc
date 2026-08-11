// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"

#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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

void WalletReminderNoticeBubbleController::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (GetBubbleView()) {
    return;
  }
  is_reshow_ = true;
  QueueOrShowBubble(/*force_show=*/true);
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
  BrowserWindowInterface* browser = tab_interface_->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }
  BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser);
  if (!browser_window) {
    return;
  }
  if (AutofillBubbleBase* bubble_view =
          browser_window->GetAutofillBubbleHandler()
              ->ShowWalletReminderNoticeBubble(web_contents(), this,
                                               is_reshow_)) {
    SetBubbleView(*bubble_view);
  }
}

}  // namespace autofill
