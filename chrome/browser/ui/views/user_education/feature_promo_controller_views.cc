// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/token.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/user_education/feature_promo_text_replacements.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

FeaturePromoControllerViews::FeaturePromoControllerViews(
    BrowserView* browser_view,
    FeaturePromoBubbleOwner* bubble_owner)
    : browser_view_(browser_view),
      bubble_owner_(bubble_owner),
      snooze_service_(std::make_unique<FeaturePromoSnoozeService>(
          browser_view->browser()->profile())),
      tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser_view->browser()->profile())) {
  DCHECK(tracker_);
}

FeaturePromoControllerViews::~FeaturePromoControllerViews() {
  if (!bubble_id_) {
    DCHECK_EQ(current_iph_feature_, nullptr);
    return;
  }

  DCHECK(current_iph_feature_);
  bubble_owner_->CloseBubble(*bubble_id_);
}

// static
FeaturePromoControllerViews* FeaturePromoControllerViews::GetForView(
    views::View* view) {
  views::Widget* widget = view->GetWidget();
  if (!widget)
    return nullptr;

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(widget->GetNativeWindow());
  if (!browser_view)
    return nullptr;

  return browser_view->feature_promo_controller();
}

bool FeaturePromoControllerViews::MaybeShowPromoWithParams(
    const base::Feature& iph_feature,
    const FeaturePromoBubbleParams& params,
    BubbleCloseCallback close_callback) {
  return MaybeShowPromoImpl(iph_feature, params, std::move(close_callback));
}

absl::optional<base::Token> FeaturePromoControllerViews::ShowCriticalPromo(
    const FeaturePromoBubbleParams& params) {
  if (promos_blocked_for_testing_)
    return absl::nullopt;

  // Don't preempt an existing critical promo.
  if (current_critical_promo_)
    return absl::nullopt;

  // If a normal bubble is showing, close it. If the promo is has
  // continued after a CloseBubbleAndContinuePromo() call, we can't stop
  // it. However we will show the critical promo anyway.
  if (current_iph_feature_ && bubble_id_)
    CloseBubble(*current_iph_feature_);

  // Snooze is not supported for critical promos.
  DCHECK(!params.allow_snooze);

  DCHECK(!bubble_id_);

  current_critical_promo_ = base::Token::CreateRandom();
  ShowPromoBubbleImpl(params);

  return current_critical_promo_;
}

void FeaturePromoControllerViews::CloseBubbleForCriticalPromo(
    const base::Token& critical_promo_id) {
  if (current_critical_promo_ != critical_promo_id)
    return;

  DCHECK(bubble_id_);
  bubble_owner_->CloseBubble(*bubble_id_);
}

bool FeaturePromoControllerViews::CriticalPromoIsShowing(
    const base::Token& critical_promo_id) const {
  return bubble_id_ && (current_critical_promo_ == critical_promo_id);
}

bool FeaturePromoControllerViews::MaybeShowPromo(
    const base::Feature& iph_feature,
    BubbleCloseCallback close_callback) {
  return MaybeShowPromoWithTextReplacements(
      iph_feature, FeaturePromoTextReplacements(), std::move(close_callback));
}

bool FeaturePromoControllerViews::MaybeShowPromoWithTextReplacements(
    const base::Feature& iph_feature,
    FeaturePromoTextReplacements text_replacements,
    BubbleCloseCallback close_callback) {
  absl::optional<FeaturePromoBubbleParams> params =
      FeaturePromoRegistry::GetInstance()->GetParamsForFeature(iph_feature,
                                                               browser_view_);
  if (!params)
    return false;

  DCHECK_GT(params->body_string_specifier, -1);
  params->body_text_raw =
      text_replacements.ApplyTo(params->body_string_specifier);
  params->body_string_specifier = -1;

  return MaybeShowPromoImpl(iph_feature, *params, std::move(close_callback));
}

void FeaturePromoControllerViews::OnUserSnooze(
    const base::Feature& iph_feature) {
  snooze_service_->OnUserSnooze(iph_feature);
}

void FeaturePromoControllerViews::OnUserDismiss(
    const base::Feature& iph_feature) {
  snooze_service_->OnUserDismiss(iph_feature);
}

bool FeaturePromoControllerViews::BubbleIsShowing(
    const base::Feature& iph_feature) const {
  return bubble_id_ && current_iph_feature_ == &iph_feature;
}

bool FeaturePromoControllerViews::CloseBubble(
    const base::Feature& iph_feature) {
  if (!BubbleIsShowing(iph_feature))
    return false;
  bubble_owner_->CloseBubble(*bubble_id_);
  return true;
}

void FeaturePromoControllerViews::UpdateBubbleForAnchorBoundsChange() {
  bubble_owner_->NotifyAnchorBoundsChanged();
}

FeaturePromoController::PromoHandle
FeaturePromoControllerViews::CloseBubbleAndContinuePromo(
    const base::Feature& iph_feature) {
  DCHECK_EQ(&iph_feature, current_iph_feature_);
  DCHECK(bubble_id_);

  bubble_owner_->CloseBubble(*std::exchange(bubble_id_, absl::nullopt));

  if (anchor_view_tracker_.view())
    anchor_view_tracker_.view()->SetProperty(kHasInProductHelpPromoKey, false);

  if (close_callback_)
    std::move(close_callback_).Run();

  // Record count of previous snoozes when the IPH gets dismissed by user
  // following the promo. e.g. clicking on relevant controls.
  int snooze_count = snooze_service_->GetSnoozeCount(iph_feature);
  base::UmaHistogramExactLinear("InProductHelp.Promos.SnoozeCountAtFollow." +
                                    std::string(iph_feature.name),
                                snooze_count,
                                snooze_service_->kUmaMaxSnoozeCount);

  return PromoHandle(weak_ptr_factory_.GetWeakPtr());
}

void FeaturePromoControllerViews::BlockPromosForTesting() {
  promos_blocked_for_testing_ = true;

  // If we own a bubble, stop the current promo.
  if (bubble_id_)
    CloseBubble(*current_iph_feature_);
}

bool FeaturePromoControllerViews::MaybeShowPromoImpl(
    const base::Feature& iph_feature,
    const FeaturePromoBubbleParams& params,
    BubbleCloseCallback close_callback) {
  if (promos_blocked_for_testing_)
    return false;

  // A normal promo cannot show if a critical promo is displayed. These
  // are not registered with |tracker_| so check here.
  if (current_critical_promo_)
    return false;

  // Temporarily turn off IPH in incognito as a concern was raised that
  // the IPH backend ignores incognito and writes to the parent profile.
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=1128728#c30
  if (browser_view_->GetProfile()->IsIncognitoProfile())
    return false;

  // Some checks should not be done in demo mode, because we absolutely want to
  // trigger the bubble if possible. Put any checks that should be bypassed in
  // demo mode in this block.
  if (!base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode)) {
    if (snooze_service_->IsBlocked(iph_feature))
      return false;
  }

  // If another bubble is showing through `bubble_owner_` it will not show ours.
  // In this case, don't query `tracker_`.
  if (bubble_owner_->AnyBubbleIsShowing())
    return false;

  if (!tracker_->ShouldTriggerHelpUI(iph_feature))
    return false;

  // If the tracker says we should trigger, but we have a promo
  // currently showing, there is a bug somewhere in here.
  DCHECK(!current_iph_feature_);
  current_iph_feature_ = &iph_feature;

  if (!ShowPromoBubbleImpl(params)) {
    // `current_iph_feature_` is needed in the call. If it fails, we must reset
    // it and also notify the backend.
    current_iph_feature_ = nullptr;
    tracker_->Dismissed(iph_feature);
    return false;
  }
  close_callback_ = std::move(close_callback);

  // Record count of previous snoozes when an IPH triggers.
  int snooze_count = snooze_service_->GetSnoozeCount(iph_feature);
  base::UmaHistogramExactLinear("InProductHelp.Promos.SnoozeCountAtTrigger." +
                                    std::string(iph_feature.name),
                                snooze_count,
                                snooze_service_->kUmaMaxSnoozeCount);

  snooze_service_->OnPromoShown(iph_feature);
  return true;
}

void FeaturePromoControllerViews::FinishContinuedPromo() {
  DCHECK(current_iph_feature_);
  DCHECK(!bubble_id_);
  tracker_->Dismissed(*current_iph_feature_);
  current_iph_feature_ = nullptr;
}

bool FeaturePromoControllerViews::ShowPromoBubbleImpl(
    const FeaturePromoBubbleParams& params) {
  // Map |params| to the bubble's create params, fetching needed strings.
  FeaturePromoBubbleView::CreateParams create_params;
  create_params.anchor_view = params.anchor_view;
  create_params.body_text =
      params.body_string_specifier != -1
          ? l10n_util::GetStringUTF16(params.body_string_specifier)
          : params.body_text_raw;
  if (params.title_string_specifier)
    create_params.title_text =
        l10n_util::GetStringUTF16(*params.title_string_specifier);

  if (params.screenreader_string_specifier && params.feature_accelerator) {
    create_params.screenreader_text = l10n_util::GetStringFUTF16(
        *params.screenreader_string_specifier,
        params.feature_accelerator->GetShortcutText());
  } else if (params.screenreader_string_specifier) {
    create_params.screenreader_text =
        l10n_util::GetStringUTF16(*params.screenreader_string_specifier);
  }

  create_params.focus_on_create = params.focus_on_create;
  create_params.persist_on_blur = params.persist_on_blur;

  create_params.arrow = params.arrow;
  create_params.preferred_width = params.preferred_width;

  create_params.timeout_default = params.timeout_default;
  create_params.timeout_short = params.timeout_short;

  if (params.allow_snooze) {
    FeaturePromoBubbleView::ButtonParams snooze_button;
    snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    snooze_button.has_border = false;
    snooze_button.callback = base::BindRepeating(
        &FeaturePromoControllerViews::OnUserSnooze,
        weak_ptr_factory_.GetWeakPtr(), *current_iph_feature_);
    create_params.buttons.push_back(std::move(snooze_button));

    FeaturePromoBubbleView::ButtonParams dismiss_button;
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
    dismiss_button.has_border = true;
    dismiss_button.callback = base::BindRepeating(
        &FeaturePromoControllerViews::OnUserDismiss,
        weak_ptr_factory_.GetWeakPtr(), *current_iph_feature_);
    create_params.buttons.push_back(std::move(dismiss_button));

    if (views::PlatformStyle::kIsOkButtonLeading)
      std::swap(create_params.buttons[0], create_params.buttons[1]);
  }

  bubble_id_ = bubble_owner_->ShowBubble(
      std::move(create_params),
      base::BindOnce(&FeaturePromoControllerViews::HandleBubbleClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!bubble_id_)
    return false;

  params.anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  anchor_view_tracker_.SetView(params.anchor_view);
  return true;
}

void FeaturePromoControllerViews::HandleBubbleClosed() {
  // We receive a callback whenever we close the bubble. However, if we closed
  // it in CloseBubbleAndContinuePromo, we don't want to run this cleanup yet.
  // There we clear the ID first.
  if (!bubble_id_)
    return;

  // Exactly one of current_iph_feature_ or current_critical_promo_ should have
  // a value.
  DCHECK_NE(current_iph_feature_ != nullptr,
            current_critical_promo_.has_value());

  bubble_id_.reset();

  if (anchor_view_tracker_.view())
    anchor_view_tracker_.view()->SetProperty(kHasInProductHelpPromoKey, false);

  if (close_callback_)
    std::move(close_callback_).Run();

  if (current_iph_feature_) {
    tracker_->Dismissed(*current_iph_feature_);
    current_iph_feature_ = nullptr;
  } else {
    current_critical_promo_.reset();
  }
}
