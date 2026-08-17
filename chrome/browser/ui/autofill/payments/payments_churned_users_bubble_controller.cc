// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "components/autofill/core/browser/payments/payments_churned_users_metrics.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#endif

namespace autofill {

DEFINE_USER_DATA(PaymentsChurnedUsersBubbleController);

PaymentsChurnedUsersBubbleController::PaymentsChurnedUsersBubbleController(
    tabs::TabInterface& tab_interface,
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

PaymentsChurnedUsersBubbleController::~PaymentsChurnedUsersBubbleController() {
  HideBubble(/*initiated_by_bubble_manager=*/false);
}

// static
PaymentsChurnedUsersBubbleController*
PaymentsChurnedUsersBubbleController::From(tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

void PaymentsChurnedUsersBubbleController::Show(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure closed_callback,
    AccountInfo account_info) {
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }
  is_reshow_ = false;
  is_accepted_ = false;
  accept_callback_ = std::move(accept_callback);
  cancel_callback_ = std::move(cancel_callback);
  closed_callback_ = std::move(closed_callback);
  account_info_ = std::move(account_info);
  should_show_icon_ = true;
  QueueOrShowBubble();
}

void PaymentsChurnedUsersBubbleController::ReshowBubble() {
  if (bubble_view()) {
    return;
  }
  is_reshow_ = true;
  QueueOrShowBubble(/*force_show=*/true);
}

void PaymentsChurnedUsersBubbleController::OnBubbleDiscarded() {
  if (closed_callback_) {
    std::move(closed_callback_).Run();
  }
}

void PaymentsChurnedUsersBubbleController::OnBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  ResetBubbleViewAndInformBubbleManager();
  autofill_metrics::LogPaymentsChurnedUsersBubbleResult(
      is_accepted_ ? PaymentsUiClosedReason::kAccepted : closed_reason);

  if (closed_reason == PaymentsUiClosedReason::kCancelled ||
      closed_reason == PaymentsUiClosedReason::kClosed) {
    should_show_icon_ = false;
  }

  UpdatePageActionIcon();

  if (is_accepted_) {
    return;
  }

  if (closed_reason == PaymentsUiClosedReason::kCancelled) {
    if (cancel_callback_) {
      std::move(cancel_callback_).Run();
    }
  } else {
    if (closed_callback_) {
      std::move(closed_callback_).Run();
    }
  }
}

AutofillEnableResurrectingPaymentsUsersTreatmentArm
PaymentsChurnedUsersBubbleController::
    GetAutofillEnableResurrectingPaymentsUsersTreatmentArm() const {
  CHECK(base::FeatureList::IsEnabled(
      features::kAutofillEnableResurrectingPaymentsUsers));
  switch (features::kAutofillEnableResurrectingPaymentsUsersTreatment.Get()) {
    case 1:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity;
    case 2:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience;
    default:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity;
  }
}

const AccountInfo& PaymentsChurnedUsersBubbleController::GetAccountInfo()
    const {
  return account_info_;
}

void PaymentsChurnedUsersBubbleController::OnAcceptButton() {
  is_accepted_ = true;
  // Although the bubble is still present, run the accept callback as the user
  // has made the decision to turn on payments autofill. The confirmation and
  // loading is strictly a visual experience, and there is no server call
  // ongoing.
  if (accept_callback_) {
    std::move(accept_callback_).Run();
  }
}

void PaymentsChurnedUsersBubbleController::ShowConfirmationBubbleView() {
  HideBubble(/*initiated_by_bubble_manager=*/false);
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab) {
    return;
  }
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }
  if (AutofillBubbleBase* bubble_view =
          AutofillBubbleHandler::Get(browser_window->GetUnownedUserDataHost())
              ->ShowPaymentsChurnedUsersConfirmationBubble(web_contents(),
                                                           this)) {
    SetBubbleView(*bubble_view);
  }
#endif
}

SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
PaymentsChurnedUsersBubbleController::GetConfirmationUiParams() const {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
      CreateForChurnedUsersAcceptanceSuccess(base::BindRepeating(
          [](content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
            tabs::TabInterface* tab =
                tabs::TabInterface::GetFromContents(web_contents);
            if (tab && tab->GetBrowserWindowInterface()) {
              chrome::ShowSettingsSubPage(tab->GetBrowserWindowInterface(),
                                          chrome::kPaymentsSubPage);
            }
#endif
          },
          web_contents()));
}

base::OnceCallback<void(PaymentsUiClosedReason)>
PaymentsChurnedUsersBubbleController::GetConfirmationBubbleClosedCallback() {
  return base::BindOnce(
      &PaymentsChurnedUsersBubbleController::OnConfirmationBubbleClosed,
      weak_ptr_factory_.GetWeakPtr());
}

void PaymentsChurnedUsersBubbleController::PrimaryPageChanged(
    content::Page& page) {
  should_show_icon_ = false;
  UpdatePageActionIcon();
  HideBubble(/*initiated_by_bubble_manager=*/false);
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

base::WeakPtr<PaymentsChurnedUsersBubbleController>
PaymentsChurnedUsersBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentsChurnedUsersBubbleController::DoShowBubble() {
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab) {
    return;
  }
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }
  AutofillBubbleBase* created_bubble =
      AutofillBubbleHandler::Get(browser_window->GetUnownedUserDataHost())
          ->ShowPaymentsChurnedUsersBubble(web_contents(), this, is_reshow_);
  if (created_bubble) {
    SetBubbleView(*created_bubble);
    autofill_metrics::LogPaymentsChurnedUsersBubbleShowResult(
        autofill_metrics::PaymentsChurnedUsersBubbleShowResult::kShown);
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
std::optional<actions::ActionId>
PaymentsChurnedUsersBubbleController::GetActionIdForPageAction() {
  return kActionShowPaymentsChurnedUsersBubble;
}

bool PaymentsChurnedUsersBubbleController::ShouldShowPageAction() {
  return should_show_icon_;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void PaymentsChurnedUsersBubbleController::OnConfirmationBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  should_show_icon_ = false;
  UpdatePageActionIcon();
  ResetBubbleViewAndInformBubbleManager();
}

}  // namespace autofill
