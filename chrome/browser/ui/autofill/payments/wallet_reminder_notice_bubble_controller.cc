// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace autofill {

DEFINE_USER_DATA(WalletReminderNoticeBubbleController);

WalletReminderNoticeBubbleController::WalletReminderNoticeBubbleController(
    tabs::TabInterface* tab_interface)
    : AutofillBubbleControllerBase(tab_interface),
      scoped_unowned_user_data_(
          CHECK_DEREF(tab_interface).GetUnownedUserDataHost(),
          *this),
      will_discard_contents_subscription_(
          tab_interface->RegisterWillDiscardContents(base::BindRepeating(
              &WalletReminderNoticeBubbleController::WillDiscardContents,
              base::Unretained(this)))) {}

WalletReminderNoticeBubbleController::~WalletReminderNoticeBubbleController() {
  if (IsShowingBubble()) {
    if (actions::ActionItem* action_item = GetActionItem()) {
      action_item->SetIsShowingBubble(false);
    }
  }
}

// static
WalletReminderNoticeBubbleController*
WalletReminderNoticeBubbleController::From(tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

void WalletReminderNoticeBubbleController::Show(
    LegalMessageLines legal_message_lines) {
  // Don't show the bubble if it's already visible or not set up.
  if (GetBubbleView() || !MaySetUpBubble()) {
    return;
  }
  legal_message_lines_ = std::move(legal_message_lines);
  is_reshow_ = false;
  QueueOrShowBubble();
}

void WalletReminderNoticeBubbleController::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (GetBubbleView()) {
    return;
  }
  is_reshow_ = true;
  QueueOrShowBubble(/*force_show=*/true);
}

std::u16string WalletReminderNoticeBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_WALLET_REMINDER_NOTICE_TITLE);
}

const LegalMessageLines&
WalletReminderNoticeBubbleController::GetLegalMessageLines() const {
  return legal_message_lines_;
}

AutofillBubbleBase* WalletReminderNoticeBubbleController::GetBubbleView()
    const {
  return bubble_view();
}

base::WeakPtr<WalletReminderNoticeBubbleController>
WalletReminderNoticeBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WalletReminderNoticeBubbleController::OnAcceptButton() {
  autofill_metrics::LogWalletReminderNoticeInteraction(
      autofill_metrics::WalletReminderNoticeInteraction::kAcknowledgedCta);
  logged_accept_button_clicked_ = true;

  if (WalletReminderNoticePageActionController* page_action_controller =
          WalletReminderNoticePageActionController::From(*GetTabInterface())) {
    page_action_controller->Hide();
  }
}

void WalletReminderNoticeBubbleController::OnLinkClicked(const GURL& url) {
  autofill_metrics::LogWalletReminderNoticeInteraction(
      autofill_metrics::WalletReminderNoticeInteraction::kClickedLink);
  logged_link_clicked_ = true;

  web_contents()->OpenURL(
      content::OpenURLParams::CreateBrowserInitiated(
          url, ui::mojom::WindowOpenDisposition::kNewForegroundTab,
          ui::PAGE_TRANSITION_LINK),
      /*navigation_handle_callback=*/{});
}

void WalletReminderNoticeBubbleController::OnBubbleClosed() {
  if (actions::ActionItem* action_item = GetActionItem()) {
    action_item->SetIsShowingBubble(false);
  }
  if (!logged_accept_button_clicked_ && !logged_link_clicked_ &&
      !bubble_hide_initiated_by_bubble_manager_) {
    autofill_metrics::LogWalletReminderNoticeInteraction(
        autofill_metrics::WalletReminderNoticeInteraction::kDismissed);
  }
  ResetBubbleViewAndInformBubbleManager();
}

void WalletReminderNoticeBubbleController::OnBubbleDiscarded() {
  // Note: If the bubble is active or queued in `BubbleManager` when a tab is
  // discarded, both `BubbleManagerImpl::WillDiscardContents()` and
  // `WalletReminderNoticeBubbleController::WillDiscardContents()` will invoke
  // `OnBubbleDiscarded()`, so this method must remain idempotent.
  legal_message_lines_.clear();
  if (WalletReminderNoticePageActionController* page_action_controller =
          WalletReminderNoticePageActionController::From(*GetTabInterface())) {
    page_action_controller->Hide();
  }
}

BubbleType WalletReminderNoticeBubbleController::GetBubbleType() const {
  return BubbleType::kWalletReminderNotice;
}

base::WeakPtr<BubbleControllerBase>
WalletReminderNoticeBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WalletReminderNoticeBubbleController::DoShowBubble() {
  BrowserWindowInterface* browser =
      GetTabInterface()->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }
  AutofillBubbleHandler* autofill_bubble_handler =
      AutofillBubbleHandler::Get(browser->GetUnownedUserDataHost());
  if (!autofill_bubble_handler) {
    return;
  }
  if (AutofillBubbleBase* bubble_view =
          autofill_bubble_handler->ShowWalletReminderNoticeBubble(
              web_contents(), this, is_reshow_)) {
    logged_accept_button_clicked_ = false;
    logged_link_clicked_ = false;
    SetBubbleView(*bubble_view);

    if (actions::ActionItem* action_item = GetActionItem()) {
      action_item->SetIsShowingBubble(true);
    }
  }
}

void WalletReminderNoticeBubbleController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  OnBubbleDiscarded();
}

actions::ActionItem* WalletReminderNoticeBubbleController::GetActionItem() {
  BrowserWindowInterface* browser_window =
      GetTabInterface()->GetBrowserWindowInterface();
  if (!browser_window) {
    return nullptr;
  }
  BrowserActions* browser_actions = BrowserActions::From(browser_window);
  if (!browser_actions) {
    return nullptr;
  }
  actions::ActionItem* root_action_item = browser_actions->root_action_item();
  if (!root_action_item) {
    return nullptr;
  }
  return actions::ActionManager::Get().FindAction(kActionWalletReminderNotice,
                                                  root_action_item);
}

}  // namespace autofill
