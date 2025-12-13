// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include <string>

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

OfferNotificationBubbleControllerImpl::
    ~OfferNotificationBubbleControllerImpl() = default;

// static
OfferNotificationBubbleController*
OfferNotificationBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  OfferNotificationBubbleControllerImpl::CreateForWebContents(web_contents);
  return OfferNotificationBubbleControllerImpl::FromWebContents(web_contents);
}

// static
OfferNotificationBubbleController* OfferNotificationBubbleController::Get(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  return OfferNotificationBubbleControllerImpl::FromWebContents(web_contents);
}

OfferNotificationBubbleControllerImpl::OfferNotificationBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<OfferNotificationBubbleControllerImpl>(
          *web_contents),
      tab_interface_(
          CHECK_DEREF(tabs::TabInterface::GetFromContents(web_contents))) {}

std::u16string OfferNotificationBubbleControllerImpl::GetWindowTitle() const {
  switch (offer_.GetOfferType()) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_LINKED_OFFER_REMINDER_TITLE);
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_GPAY_PROMO_CODE_OFFERS_REMINDER_TITLE);
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
  }
}

std::u16string OfferNotificationBubbleControllerImpl::GetOkButtonLabel() const {
  DCHECK_EQ(offer_.GetOfferType(),
            AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER);
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_OFFERS_REMINDER_POSITIVE_BUTTON_LABEL);
}

std::u16string
OfferNotificationBubbleControllerImpl::GetPromoCodeButtonTooltip() const {
  return l10n_util::GetStringUTF16(
      promo_code_button_clicked_
          ? IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_CLICKED
          : IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_NORMAL);
}

AutofillBubbleBase*
OfferNotificationBubbleControllerImpl::GetOfferNotificationBubbleView() const {
  return bubble_view();
}

const CreditCard* OfferNotificationBubbleControllerImpl::GetLinkedCard() const {
  if (card_.has_value()) {
    return &(*card_);
  }

  return nullptr;
}

const AutofillOfferData* OfferNotificationBubbleControllerImpl::GetOffer()
    const {
  return &offer_;
}

bool OfferNotificationBubbleControllerImpl::IsIconVisible() const {
  return bubble_state_ != BubbleState::kHidden;
}

void OfferNotificationBubbleControllerImpl::OnBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  ResetBubbleViewAndInformBubbleManager();
  promo_code_button_clicked_ = false;
  UpdatePageActionIcon();
}

void OfferNotificationBubbleControllerImpl::ShowOfferNotificationIfApplicable(
    const AutofillOfferData& offer,
    const CreditCard* card,
    const OfferNotificationOptions& options) {
  // If this is not the bubble's first show, and offer to be shown has not
  // changed, and it has not been shown for more than
  // kAutofillBubbleSurviveNavigationTime, do not dismiss the bubble.
  if (offer_.GetOfferType() != AutofillOfferData::OfferType::UNKNOWN &&
      offer_ == offer && bubble_shown_timestamp_.has_value() &&
      AutofillClock::Now() - *bubble_shown_timestamp_ <
          kAutofillBubbleSurviveNavigationTime) {
    return;
  }

  if (!MaySetUpBubble()) {
    return;
  }

  // Hides the old bubble. Sets bubble_state_ to show icon here since we are
  // going to show another bubble anyway.
  HideBubbleAndClearTimestamp(/*should_show_icon=*/true);

  SetupOfferNotification(offer, card);

  if (options.show_notification_automatically) {
    QueueOrShowBubble();
  } else {
    HideBubbleAndClearTimestamp(/*should_show_icon=*/true);
  }
}

void OfferNotificationBubbleControllerImpl::SetupOfferNotification(
    AutofillOfferData offer,
    const CreditCard* card) {
  was_bubble_shown_ = false;
  offer_ = std::move(offer);

  DCHECK(IsIconVisible());

  if (card) {
    card_ = *card;
  }
  is_user_gesture_ = false;
}

void OfferNotificationBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());
  if (bubble_view()) {
    return;
  }

  is_user_gesture_ = true;

  QueueOrShowBubble(/*force_show=*/true);
}

void OfferNotificationBubbleControllerImpl::DismissNotification() {
  HideBubbleAndClearTimestamp(/*should_show_icon=*/false);
}

void OfferNotificationBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (IsBubbleManagerEnabled()) {
    if (visibility == content::Visibility::HIDDEN) {
      if (bubble_state_ != BubbleState::kShowingIcon) {
        bubble_state_ = BubbleState::kHidden;
      }

      // BubbleManager will hide the bubble.
      bubble_shown_timestamp_ = std::nullopt;
    }
    return;
  }

  if (visibility == content::Visibility::VISIBLE && !bubble_view() &&
      bubble_state_ == BubbleState::kShowingIconAndBubble) {
    QueueOrShowBubble();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubbleAndClearTimestamp(bubble_state_ == BubbleState::kShowingIcon);
  }
  UpdatePageActionIcon();
}

#if !BUILDFLAG(IS_ANDROID)
std::optional<actions::ActionId>
OfferNotificationBubbleControllerImpl::GetActionIdForPageAction() {
  return kActionOffersAndRewardsForPage;
}

bool OfferNotificationBubbleControllerImpl::ShouldShowPageAction() {
  return IsIconVisible();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void OfferNotificationBubbleControllerImpl::DoShowBubble() {
  bubble_state_ = BubbleState::kShowingIconAndBubble;
  UpdatePageActionIcon();

  // Don't show bubble yet if web content is not active (bubble will instead be
  // shown when web content become visible and active).
  if (!IsWebContentsActive()) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  SetBubbleView(*browser->window()
                     ->GetAutofillBubbleHandler()
                     ->ShowOfferNotificationBubble(web_contents(), this,
                                                   is_user_gesture_));
  DCHECK(bubble_view());

  // Update |bubble_state_| after bubble is shown once. In OnVisibilityChanged()
  // we display the bubble if the the state is kShowingIconAndBubble. Once we
  // open the bubble here once, we set |bubble_state_| to kShowingIcon to make
  // sure further OnVisibilityChanged() don't trigger opening the bubble because
  // we don't want to re-show it every time the web contents become visible.
  bubble_state_ = BubbleState::kShowingIcon;
  bubble_shown_timestamp_ = AutofillClock::Now();

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

BubbleType OfferNotificationBubbleControllerImpl::GetBubbleType() const {
  return BubbleType::kOfferNotification;
}

base::WeakPtr<BubbleControllerBase>
OfferNotificationBubbleControllerImpl::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool OfferNotificationBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser) {
    return false;
  }

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

void OfferNotificationBubbleControllerImpl::HideBubbleAndClearTimestamp(
    bool should_show_icon) {
  bubble_state_ =
      should_show_icon ? BubbleState::kShowingIcon : BubbleState::kHidden;
  UpdatePageActionIcon();
  HideBubble(/*initiated_by_bubble_manager=*/false);
  bubble_shown_timestamp_ = std::nullopt;
}

void OfferNotificationBubbleControllerImpl::UpdatePageActionIcon() {
  // Page action icons do not exist for Android.
#if !BUILDFLAG(IS_ANDROID)
  AutofillBubbleControllerBase::UpdatePageActionIcon();

  if (!IsPageActionMigrated(*GetPageActionIconType()) ||
      web_contents()->IsBeingDestroyed()) {
    return;
  }
  actions::ActionId action_id = *GetActionIdForPageAction();
  auto* action = actions::ActionManager::Get().FindAction(
      action_id, tab_interface_->GetBrowserWindowInterface()
                     ->GetActions()
                     ->root_action_item());
  action->SetEnabled(ShouldShowPageAction());
#endif  // BUILDFLAG(IS_ANDROID)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationBubbleControllerImpl);

}  // namespace autofill
