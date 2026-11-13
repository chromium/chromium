// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "components/autofill/core/browser/payments/payments_churned_users_metrics.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#endif

namespace autofill {

DEFINE_USER_DATA(PaymentsChurnedUsersBubbleController);

PaymentsChurnedUsersBubbleController::PaymentsChurnedUsersBubbleController(
    tabs::TabInterface* tab_interface)
    : AutofillBubbleControllerBase(
          tab_interface,
          PageActionConfig{.action_id = kActionShowPaymentsChurnedUsersBubble,
                           .should_manage_visibility = true,
                           .should_sync_bubble_showing_state = true}),
      scoped_unowned_user_data_(
          CHECK_DEREF(tab_interface).GetUnownedUserDataHost(),
          *this) {}

PaymentsChurnedUsersBubbleController::~PaymentsChurnedUsersBubbleController() {
  HideBubble(/*initiated_by_bubble_manager=*/false);
}

// static
PaymentsChurnedUsersBubbleController*
PaymentsChurnedUsersBubbleController::From(tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

void PaymentsChurnedUsersBubbleController::Show(
    base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback,
    AccountInfo account_info) {
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }
  is_reshow_ = false;
  state_ = BubbleState::kPrompt;
  closed_callback_ = std::move(closed_callback);
  account_info_ = std::move(account_info);
  QueueOrShowBubble();
}

void PaymentsChurnedUsersBubbleController::ReshowBubble() {
  if (bubble_view() || !CanBeReshown()) {
    return;
  }
  is_reshow_ = true;
  QueueOrShowBubble(/*force_show=*/true);
}

void PaymentsChurnedUsersBubbleController::OnBubbleDiscarded() {
  state_ = BubbleState::kInactive;
  UpdatePageActionIcon();
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kNotInteracted);
  }
}

void PaymentsChurnedUsersBubbleController::OnBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  ResetBubbleViewAndInformBubbleManager();
  const bool was_accepted = state_ == BubbleState::kPendingConfirmation;
  autofill_metrics::LogPaymentsChurnedUsersUiResult(
      was_accepted ? PaymentsUiClosedReason::kAccepted : closed_reason);

  if (closed_reason == PaymentsUiClosedReason::kCancelled ||
      closed_reason == PaymentsUiClosedReason::kClosed) {
    state_ = BubbleState::kInactive;
  }

  UpdatePageActionIcon();

  if (was_accepted) {
    return;
  }

  if (closed_callback_) {
    std::move(closed_callback_).Run(closed_reason);
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
  state_ = BubbleState::kPendingConfirmation;
  // Although the bubble is still present, run the closed callback with
  // `kAccepted` as the user has made the decision to turn on payments autofill.
  // The confirmation and loading is strictly a visual experience, and there is
  // no server call ongoing.
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kAccepted);
  }
}

void PaymentsChurnedUsersBubbleController::ShowConfirmationBubbleView() {
  if (state_ != BubbleState::kPendingConfirmation) {
    return;
  }
  DoNotShowNextQueuedBubbleGuard guard = DoNotShowNextQueuedBubble();
  HideBubble(/*initiated_by_bubble_manager=*/false);
  state_ = BubbleState::kConfirmation;
  QueueOrShowBubble();
}

SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
PaymentsChurnedUsersBubbleController::GetConfirmationUiParams() {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
      CreateForChurnedUsersAcceptanceSuccess(base::BindRepeating(
          [](tabs::TabInterface* tab) {
#if !BUILDFLAG(IS_ANDROID)
            if (tab && tab->GetBrowserWindowInterface()) {
              chrome::ShowSettingsSubPage(tab->GetBrowserWindowInterface(),
                                          chrome::kPaymentsSubPage);
            }
#endif
          },
          GetTabInterface()));
}

base::OnceCallback<void(PaymentsUiClosedReason)>
PaymentsChurnedUsersBubbleController::GetConfirmationBubbleClosedCallback() {
  return base::BindOnce(
      &PaymentsChurnedUsersBubbleController::OnConfirmationBubbleClosed,
      weak_ptr_factory_.GetWeakPtr());
}

void PaymentsChurnedUsersBubbleController::PrimaryPageChanged(
    content::Page& page) {
  HideBubble(/*initiated_by_bubble_manager=*/false);
  state_ = BubbleState::kInactive;
  UpdatePageActionIcon();
}

bool PaymentsChurnedUsersBubbleController::CanBeReshown() const {
  switch (state_) {
    case BubbleState::kInactive:
    case BubbleState::kPendingConfirmation:
      return false;
    case BubbleState::kPrompt:
      return true;
    case BubbleState::kConfirmation:
      return !IsShowingBubble();
  }
  NOTREACHED();
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
  AutofillBubbleHandler* const handler = GetAutofillBubbleHandler();
  if (!handler) {
    return;
  }
  switch (state_) {
    case BubbleState::kInactive:
    case BubbleState::kPendingConfirmation:
      NOTREACHED();
    case BubbleState::kPrompt:
      if (AutofillBubbleBase* const created_bubble =
              handler->ShowPaymentsChurnedUsersBubble(web_contents(), this,
                                                      is_reshow_)) {
        SetBubbleView(*created_bubble);
        autofill_metrics::LogPaymentsChurnedUsersUiShowResult(
            autofill_metrics::PaymentsChurnedUsersUiShowResult::kShown);
      }
      return;
    case BubbleState::kConfirmation:
      if (AutofillBubbleBase* const created_bubble =
              handler->ShowPaymentsChurnedUsersConfirmationBubble(
                  web_contents(), this)) {
        SetBubbleView(*created_bubble);
      }
      return;
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
bool PaymentsChurnedUsersBubbleController::ShouldShowPageAction() {
  return state_ != BubbleState::kInactive;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void PaymentsChurnedUsersBubbleController::OnConfirmationBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  state_ = BubbleState::kInactive;
  UpdatePageActionIcon();
  ResetBubbleViewAndInformBubbleManager();
}

}  // namespace autofill
