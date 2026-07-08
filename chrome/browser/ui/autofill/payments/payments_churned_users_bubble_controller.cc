// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif

namespace autofill {

DEFINE_USER_DATA(PaymentsChurnedUsersBubbleController);

PaymentsChurnedUsersBubbleController::PaymentsChurnedUsersBubbleController(
    tabs::TabInterface& tab_interface,
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

PaymentsChurnedUsersBubbleController::~PaymentsChurnedUsersBubbleController() =
    default;

// static
PaymentsChurnedUsersBubbleController*
PaymentsChurnedUsersBubbleController::From(tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

void PaymentsChurnedUsersBubbleController::Show() {
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }
  is_reshow_ = false;
  QueueOrShowBubble();
}

void PaymentsChurnedUsersBubbleController::ReshowBubble() {
  if (bubble_view()) {
    return;
  }
  is_reshow_ = true;
  QueueOrShowBubble(/*force_show=*/true);
}

void PaymentsChurnedUsersBubbleController::OnBubbleDiscarded() {}
void PaymentsChurnedUsersBubbleController::OnBubbleClosed() {
  ResetBubbleViewAndInformBubbleManager();
  UpdatePageActionIcon();
}

bool PaymentsChurnedUsersBubbleController::CanBeReshown() const {
  return true;
}

BubbleType PaymentsChurnedUsersBubbleController::GetBubbleType() const {
  return BubbleType::kPaymentsChurnedUsers;
}

base::WeakPtr<BubbleControllerBase>
PaymentsChurnedUsersBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentsChurnedUsersBubbleController::DoShowBubble() {
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          web_contents());
  if (!browser) {
    return;
  }
  BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser);
  if (!browser_window) {
    return;
  }
  if (AutofillBubbleBase* bubble_view =
          browser_window->GetAutofillBubbleHandler()
              ->ShowPaymentsChurnedUsersBubble(web_contents(), this,
                                               is_reshow_)) {
    SetBubbleView(*bubble_view);
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
std::optional<actions::ActionId>
PaymentsChurnedUsersBubbleController::GetActionIdForPageAction() {
  return kActionShowPaymentsChurnedUsersBubble;
}

bool PaymentsChurnedUsersBubbleController::ShouldShowPageAction() {
  return true;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill
