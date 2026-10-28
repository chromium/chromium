// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include <string>

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"
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
#include "chrome/browser/ui/page_action/page_action_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

namespace {
bool g_ignore_window_activation_for_testing = false;
}  // namespace

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
  return base::UTF8ToUTF16(offer_.GetOfferRewardAmount());
}

AutofillBubbleBase*
OfferNotificationBubbleControllerImpl::GetOfferNotificationBubbleView() const {
  return bubble_view();
}

const AutofillOfferData* OfferNotificationBubbleControllerImpl::GetOffer()
    const {
  return &offer_;
}

bool OfferNotificationBubbleControllerImpl::IsIconVisible() const {
  return is_icon_visible_;
}

void OfferNotificationBubbleControllerImpl::OnBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  ResetBubbleViewAndInformBubbleManager();

  autofill_metrics::LogOfferNotificationBubbleResult(closed_reason);

  UpdatePageActionIcon();
}

void OfferNotificationBubbleControllerImpl::ShowOfferNotificationIfApplicable(
    const AutofillOfferData& offer,
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

  // Hides the old bubble. Sets is_icon_visible_ to show icon here since we are
  // going to show another bubble anyway.
  HideBubbleAndClearTimestamp(/*should_show_icon=*/true);

  SetupOfferNotification(offer);

  if (options.show_notification_automatically) {
    QueueOrShowBubble();
  } else {
    HideBubbleAndClearTimestamp(/*should_show_icon=*/true);
  }
}

void OfferNotificationBubbleControllerImpl::SetupOfferNotification(
    AutofillOfferData offer) {
  was_bubble_shown_ = false;
  offer_ = std::move(offer);

  CHECK(IsIconVisible(), base::NotFatalUntil::M162);

  is_user_gesture_ = false;
}

void OfferNotificationBubbleControllerImpl::ReshowBubble() {
  CHECK(IsIconVisible(), base::NotFatalUntil::M162);
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
  AutofillBubbleControllerBase::OnVisibilityChanged(visibility);
  if (visibility == content::Visibility::HIDDEN) {
    bubble_shown_timestamp_ = std::nullopt;
  }
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
  is_icon_visible_ = true;
  UpdatePageActionIcon();

  // Don't show bubble yet if web content is not active (bubble will instead be
  // shown when web content become visible and active).
  if (!IsWebContentsActive()) {
    return;
  }

  AutofillBubbleHandler* autofill_bubble_handler = GetAutofillBubbleHandler();
  SetBubbleView(*autofill_bubble_handler->ShowOfferNotificationBubble(
      web_contents(), this, is_user_gesture_));
  CHECK(bubble_view(), base::NotFatalUntil::M162);

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

// static
base::AutoReset<bool>
OfferNotificationBubbleControllerImpl::IgnoreWindowActivationForTesting() {
  return base::AutoReset<bool>(&g_ignore_window_activation_for_testing, true);
}

bool OfferNotificationBubbleControllerImpl::IsWebContentsActive() {
  if (g_ignore_window_activation_for_testing) {
    return tab_interface_->IsActivated();
  }

  BrowserWindowInterface* active_browser =
      GlobalBrowserCollection::GetInstance()->GetActiveBrowser();
  if (!active_browser) {
    return false;
  }

  return active_browser->GetTabStripModel()->GetActiveWebContents() ==
         web_contents();
}

void OfferNotificationBubbleControllerImpl::HideBubbleAndClearTimestamp(
    bool should_show_icon) {
  is_icon_visible_ = should_show_icon;
  UpdatePageActionIcon();
  HideBubble(/*initiated_by_bubble_manager=*/false);
  bubble_shown_timestamp_ = std::nullopt;
}

// TODO(crbug.com/546252995): Investigate whether this override is needed, since
// AutofillBubbleControllerBase::UpdatePageActionIcon() already updates the
// page action and no other bubble overrides it.
void OfferNotificationBubbleControllerImpl::UpdatePageActionIcon() {
  // Page action icons do not exist for Android.
#if !BUILDFLAG(IS_ANDROID)
  AutofillBubbleControllerBase::UpdatePageActionIcon();

  if (web_contents()->IsBeingDestroyed()) {
    return;
  }
  // The tab has no browser window while it is being detached, e.g. when it is
  // closed.
  BrowserWindowInterface* const browser =
      tab_interface_->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }
  actions::ActionId action_id = *GetActionIdForPageAction();
  auto* action = actions::ActionManager::Get().FindAction(
      action_id, BrowserActions::From(browser)->root_action_item());
  action->SetEnabled(ShouldShowPageAction());
#endif  // BUILDFLAG(IS_ANDROID)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationBubbleControllerImpl);

}  // namespace autofill
