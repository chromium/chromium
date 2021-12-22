// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/token.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

namespace {

views::BubbleBorder::Arrow MapToBubbleBorderArrow(
    FeaturePromoSpecification::BubbleArrow arrow) {
  switch (arrow) {
    case FeaturePromoSpecification::BubbleArrow::kNone:
      return views::BubbleBorder::Arrow::NONE;
    case FeaturePromoSpecification::BubbleArrow::kTopLeft:
      return views::BubbleBorder::Arrow::TOP_LEFT;
    case FeaturePromoSpecification::BubbleArrow::kTopRight:
      return views::BubbleBorder::Arrow::TOP_RIGHT;
    case FeaturePromoSpecification::BubbleArrow::kBottomLeft:
      return views::BubbleBorder::Arrow::BOTTOM_LEFT;
    case FeaturePromoSpecification::BubbleArrow::kBottomRight:
      return views::BubbleBorder::Arrow::BOTTOM_RIGHT;
    case FeaturePromoSpecification::BubbleArrow::kLeftTop:
      return views::BubbleBorder::Arrow::LEFT_TOP;
    case FeaturePromoSpecification::BubbleArrow::kRightTop:
      return views::BubbleBorder::Arrow::RIGHT_TOP;
    case FeaturePromoSpecification::BubbleArrow::kLeftBottom:
      return views::BubbleBorder::Arrow::LEFT_BOTTOM;
    case FeaturePromoSpecification::BubbleArrow::kRightBottom:
      return views::BubbleBorder::Arrow::RIGHT_BOTTOM;
    case FeaturePromoSpecification::BubbleArrow::kTopCenter:
      return views::BubbleBorder::Arrow::TOP_CENTER;
    case FeaturePromoSpecification::BubbleArrow::kBottomCenter:
      return views::BubbleBorder::Arrow::BOTTOM_CENTER;
    case FeaturePromoSpecification::BubbleArrow::kLeftCenter:
      return views::BubbleBorder::Arrow::LEFT_CENTER;
    case FeaturePromoSpecification::BubbleArrow::kRightCenter:
      return views::BubbleBorder::Arrow::RIGHT_CENTER;
  }
}

std::u16string GetScreenReaderAcceleratorPrompt(
    BrowserView* browser_view,
    FeaturePromoSpecification::PromoType promo_type,
    const views::View* anchor_view,
    bool is_critical_promo) {
  // No message is required as this is a background bubble with a
  // screen reader-specific prompt and will dismiss itself.
  if (promo_type == FeaturePromoSpecification::PromoType::kToast)
    return std::u16string();

  ui::Accelerator accelerator;
  std::u16string accelerator_text;
  if (browser_view->GetAccelerator(IDC_FOCUS_NEXT_PANE, &accelerator)) {
    accelerator_text = accelerator.GetShortcutText();
  } else {
    NOTREACHED();
  }

  // Present the user with the full help bubble navigation shortcut.
  if (anchor_view->IsAccessibilityFocusable()) {
    return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_TOGGLE_DESCRIPTION,
                                      accelerator_text);
  }

  // If the bubble starts focused and focus cannot traverse to the anchor view,
  // do not use a promo.
  if (is_critical_promo)
    return std::u16string();

  // Present the user with an abridged help bubble navigation shortcut.
  return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_DESCRIPTION,
                                    accelerator_text);
}

}  // namespace

// static
bool FeaturePromoControllerViews::active_window_check_blocked_for_testing =
    false;

FeaturePromoControllerViews::FeaturePromoControllerViews(
    BrowserView* browser_view,
    FeaturePromoBubbleOwner* bubble_owner,
    TutorialService* tutorial_service)
    : browser_view_(browser_view),
      bubble_owner_(bubble_owner),
      snooze_service_(std::make_unique<FeaturePromoSnoozeService>(
          browser_view->browser()->profile())),
      tutorial_service_(tutorial_service),
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

bool FeaturePromoControllerViews::MaybeShowPromoFromSpecification(
    const FeaturePromoSpecification& spec,
    views::View* anchor_view,
    FeaturePromoSpecification::StringReplacements text_replacements,
    BubbleCloseCallback close_callback) {
  return MaybeShowPromoImpl(spec, anchor_view, std::move(text_replacements),
                            std::move(close_callback));
}

absl::optional<base::Token> FeaturePromoControllerViews::ShowCriticalPromo(
    const FeaturePromoSpecification& spec,
    views::View* anchor_view,
    FeaturePromoSpecification::StringReplacements body_text_replacements) {
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

  // Some promo types are not supported for critical promos.
  DCHECK_NE(FeaturePromoSpecification::PromoType::kSnooze, spec.promo_type());
  DCHECK_NE(FeaturePromoSpecification::PromoType::kTutorial, spec.promo_type());

  DCHECK(!bubble_id_);

  const bool screen_reader_available = CheckScreenReaderPromptAvailable();

  current_critical_promo_ = base::Token::CreateRandom();
  ShowPromoBubbleImpl(spec, anchor_view, std::move(body_text_replacements),
                      screen_reader_available, /* is_critical_promo =*/true);

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

bool FeaturePromoControllerViews::DismissNonCriticalBubbleInRegion(
    const gfx::Rect& screen_bounds) {
  if (!bubble_id_ || current_critical_promo_ ||
      !bubble_owner_->BubbleIsShowing(bubble_id_.value())) {
    return false;
  }

  if (!screen_bounds.Intersects(
          bubble_owner_->GetBubbleBoundsInScreen(bubble_id_.value()))) {
    return false;
  }

  bubble_owner_->CloseBubble(bubble_id_.value());
  return true;
}

bool FeaturePromoControllerViews::MaybeShowPromo(
    const base::Feature& iph_feature,
    FeaturePromoSpecification::StringReplacements text_replacements,
    BubbleCloseCallback close_callback) {
  const FeaturePromoSpecification* spec =
      FeaturePromoRegistry::GetInstance()->GetParamsForFeature(iph_feature);
  if (!spec)
    return false;

  DCHECK_EQ(&iph_feature, spec->feature());
  DCHECK(spec->anchor_element_id());

  // Fetch the anchor element. For now, assume all elements are Views.
  auto* const anchor_element = spec->GetAnchorElement(
      views::ElementTrackerViews::GetContextForView(browser_view_));

  if (!anchor_element)
    return false;

  if (!anchor_element->IsA<views::TrackedElementViews>()) {
    NOTREACHED();
    return false;
  }

  return MaybeShowPromoImpl(
      *spec, anchor_element->AsA<views::TrackedElementViews>()->view(),
      std::move(text_replacements), std::move(close_callback));
}

void FeaturePromoControllerViews::OnUserSnooze(
    const base::Feature& iph_feature) {
  snooze_service_->OnUserSnooze(iph_feature);
}

void FeaturePromoControllerViews::OnUserDismiss(
    const base::Feature& iph_feature) {
  if (snooze_service_)
    snooze_service_->OnUserDismiss(iph_feature);
}

void FeaturePromoControllerViews::OnTutorialStart(
    const base::Feature& iph_feature,
    TutorialIdentifier tutorial_id) {
  if (snooze_service_)
    snooze_service_->OnUserDismiss(iph_feature);
  if (!bubble_id_ || !bubble_owner_->BubbleIsShowing(*bubble_id_)) {
    StartTutorial(tutorial_id);
  } else {
    pending_tutorial_ = tutorial_id;
  }
}

void FeaturePromoControllerViews::StartTutorial(
    TutorialIdentifier tutorial_id) {
  if (!browser_view_)
    return;
  tutorial_service_->StartTutorial(
      tutorial_id,
      views::ElementTrackerViews::GetContextForView(browser_view_));
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

base::WeakPtr<FeaturePromoController>
FeaturePromoControllerViews::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
void FeaturePromoControllerViews::BlockActiveWindowCheckForTesting() {
  active_window_check_blocked_for_testing = true;
}

// static
bool FeaturePromoControllerViews::IsActiveWindowCheckBlockedForTesting() {
  return active_window_check_blocked_for_testing;
}

void FeaturePromoControllerViews::BlockPromosForTesting() {
  promos_blocked_for_testing_ = true;

  // If we own a bubble, stop the current promo.
  if (bubble_id_)
    CloseBubble(*current_iph_feature_);
}

bool FeaturePromoControllerViews::MaybeShowPromoImpl(
    const FeaturePromoSpecification& spec,
    views::View* anchor_view,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
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

  // Don't show IPH if the anchor view is in an inactive window
  if (!active_window_check_blocked_for_testing &&
      !anchor_view->GetWidget()->ShouldPaintAsActive())
    return false;

  // Some checks should not be done in demo mode, because we absolutely want to
  // trigger the bubble if possible. Put any checks that should be bypassed in
  // demo mode in this block.
  const base::Feature& feature = *spec.feature();
  if (!base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode)) {
    if (snooze_service_->IsBlocked(feature))
      return false;
  }

  // If another bubble is showing through `bubble_owner_` it will not show ours.
  // In this case, don't query `tracker_`.
  if (bubble_owner_->AnyBubbleIsShowing())
    return false;

  // TODO(crbug.com/1258216): Currently this must be called before
  // ShouldTriggerHelpUI() below. See bug for details.
  const bool screen_reader_available = CheckScreenReaderPromptAvailable();

  if (!tracker_->ShouldTriggerHelpUI(feature))
    return false;

  // If the tracker says we should trigger, but we have a promo
  // currently showing, there is a bug somewhere in here.
  DCHECK(!current_iph_feature_);
  current_iph_feature_ = &feature;

  if (!ShowPromoBubbleImpl(spec, anchor_view, std::move(body_text_replacements),
                           screen_reader_available,
                           /* is_critical_promo =*/false)) {
    // `current_iph_feature_` is needed in the call. If it fails, we must reset
    // it and also notify the backend.
    current_iph_feature_ = nullptr;
    tracker_->Dismissed(feature);
    return false;
  }
  close_callback_ = std::move(close_callback);

  // Record count of previous snoozes when an IPH triggers.
  int snooze_count = snooze_service_->GetSnoozeCount(feature);
  base::UmaHistogramExactLinear(
      "InProductHelp.Promos.SnoozeCountAtTrigger." + std::string(feature.name),
      snooze_count, snooze_service_->kUmaMaxSnoozeCount);

  snooze_service_->OnPromoShown(feature);
  return true;
}

void FeaturePromoControllerViews::FinishContinuedPromo() {
  DCHECK(current_iph_feature_);
  DCHECK(!bubble_id_);
  tracker_->Dismissed(*current_iph_feature_);
  current_iph_feature_ = nullptr;
}

FeaturePromoBubbleView::CreateParams
FeaturePromoControllerViews::GetBaseCreateParams(
    const FeaturePromoSpecification& spec,
    views::View* anchor_view,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    bool is_critical_promo) {
  FeaturePromoBubbleView::CreateParams create_params;
  create_params.anchor_view = anchor_view;
  create_params.body_text = l10n_util::GetStringFUTF16(
      spec.bubble_body_string_id(), std::move(body_text_replacements), nullptr);
  create_params.title_text = spec.bubble_title_text();
  if (spec.screen_reader_string_id()) {
    create_params.screenreader_text =
        spec.screen_reader_accelerator()
            ? l10n_util::GetStringFUTF16(spec.screen_reader_string_id(),
                                         spec.screen_reader_accelerator()
                                             .GetAccelerator(browser_view_)
                                             .GetShortcutText())
            : l10n_util::GetStringUTF16(spec.screen_reader_string_id());
  }
  create_params.body_icon = spec.bubble_icon();
  create_params.arrow = MapToBubbleBorderArrow(spec.bubble_arrow());
  create_params.focus_on_create = is_critical_promo;
  create_params.persist_on_blur = !is_critical_promo;

  // Critical promos don't time out.
  if (is_critical_promo)
    create_params.timeout = base::Seconds(0);

  return create_params;
}

bool FeaturePromoControllerViews::ShowPromoBubbleImpl(
    const FeaturePromoSpecification& spec,
    views::View* anchor_view,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    bool screen_reader_promo,
    bool is_critical_promo) {
  // For critical promos, there should be no current feature.
  // For non-critical promos, we should already have installed the promo's
  // feature ID.
  DCHECK_EQ(is_critical_promo, !current_iph_feature_);
  DCHECK(is_critical_promo || current_iph_feature_ == spec.feature());

  FeaturePromoBubbleView::CreateParams create_params = GetBaseCreateParams(
      spec, anchor_view, std::move(body_text_replacements), is_critical_promo);

  if (spec.promo_type() == FeaturePromoSpecification::PromoType::kSnooze &&
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopSnoozeFeature)) {
    CHECK(spec.feature());
    create_params.buttons = CreateSnoozeButtons(*spec.feature());
  } else if (spec.promo_type() ==
             FeaturePromoSpecification::PromoType::kTutorial) {
    CHECK(spec.feature());
    create_params.buttons =
        CreateTutorialButtons(*spec.feature(), spec.tutorial_id());
    create_params.has_close_button = true;
  } else {
    create_params.has_close_button = true;
  }

  // Feature isn't present for some critical promos.
  if (spec.feature()) {
    create_params.dismiss_callback =
        base::BindRepeating(&FeaturePromoControllerViews::OnUserDismiss,
                            weak_ptr_factory_.GetWeakPtr(), *spec.feature());
  }

  if (CheckScreenReaderPromptAvailable()) {
    create_params.keyboard_navigation_hint = GetScreenReaderAcceleratorPrompt(
        browser_view_, spec.promo_type(), anchor_view, is_critical_promo);
  }
  const bool had_screen_reader_promo =
      !create_params.keyboard_navigation_hint.empty();

  bubble_id_ = bubble_owner_->ShowBubble(
      std::move(create_params),
      base::BindOnce(&FeaturePromoControllerViews::HandleBubbleClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!bubble_id_)
    return false;

  // Record that the focus help message was actually read to the user. See the
  // note in MaybeShowPromoImpl().
  // TODO(crbug.com/1258216): Rewrite this when we have the ability for FE
  // promos to ignore other active promos.
  if (had_screen_reader_promo) {
    tracker_->NotifyEvent(
        feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead);
  }

  anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  anchor_view_tracker_.SetView(anchor_view);
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

  if (!pending_tutorial_.empty()) {
    StartTutorial(pending_tutorial_);
    pending_tutorial_ = TutorialIdentifier();
  }
}

bool FeaturePromoControllerViews::CheckScreenReaderPromptAvailable() const {
  if (!ui::AXPlatformNode::GetAccessibilityMode().has_mode(
          ui::AXMode::kScreenReader)) {
    return false;
  }

  // If we're in demo mode and screen reader is on, always play the demo
  // without querying the FE backend, since the backend will return false for
  // all promos other than the one that's being demoed. If we didn't have this
  // code the screen reader prompt would never play.
  if (base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode))
    return true;

  if (!tracker_->ShouldTriggerHelpUI(
          feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature)) {
    return false;
  }

  // TODO(crbug.com/1258216): Once we have our answer, immediately dismiss
  // so that this doesn't interfere with actually showing the bubble. This
  // dismiss can be moved elsewhere once we support concurrency.
  tracker_->Dismissed(
      feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature);

  return true;
}

std::vector<FeaturePromoBubbleView::ButtonParams>
FeaturePromoControllerViews::CreateSnoozeButtons(const base::Feature& feature) {
  std::vector<FeaturePromoBubbleView::ButtonParams> buttons;

  FeaturePromoBubbleView::ButtonParams snooze_button;
  snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
  snooze_button.has_border = false;
  snooze_button.callback =
      base::BindRepeating(&FeaturePromoControllerViews::OnUserSnooze,
                          weak_ptr_factory_.GetWeakPtr(), feature);
  buttons.push_back(std::move(snooze_button));

  FeaturePromoBubbleView::ButtonParams dismiss_button;
  dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
  dismiss_button.has_border = true;
  dismiss_button.callback =
      base::BindRepeating(&FeaturePromoControllerViews::OnUserDismiss,
                          weak_ptr_factory_.GetWeakPtr(), feature);
  buttons.push_back(std::move(dismiss_button));

  if (views::PlatformStyle::kIsOkButtonLeading)
    std::swap(buttons[0], buttons[1]);

  return buttons;
}

std::vector<FeaturePromoBubbleView::ButtonParams>
FeaturePromoControllerViews::CreateTutorialButtons(
    const base::Feature& feature,
    TutorialIdentifier tutorial_id) {
  std::vector<FeaturePromoBubbleView::ButtonParams> buttons;

  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopSnoozeFeature)) {
    FeaturePromoBubbleView::ButtonParams snooze_button;
    snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    snooze_button.has_border = false;
    snooze_button.callback =
        base::BindRepeating(&FeaturePromoControllerViews::OnUserSnooze,
                            weak_ptr_factory_.GetWeakPtr(), feature);
    buttons.push_back(std::move(snooze_button));
  } else {
    FeaturePromoBubbleView::ButtonParams dismiss_button;
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
    dismiss_button.has_border = false;
    dismiss_button.callback =
        base::BindRepeating(&FeaturePromoControllerViews::OnUserDismiss,
                            weak_ptr_factory_.GetWeakPtr(), feature);
    buttons.push_back(std::move(dismiss_button));
  }

  FeaturePromoBubbleView::ButtonParams tutorial_button;
  tutorial_button.text =
      l10n_util::GetStringUTF16(IDS_PROMO_SHOW_TUTORIAL_BUTTON);
  tutorial_button.has_border = true;
  tutorial_button.callback =
      base::BindRepeating(&FeaturePromoControllerViews::OnTutorialStart,
                          weak_ptr_factory_.GetWeakPtr(), feature, tutorial_id);
  buttons.push_back(std::move(tutorial_button));

  if (views::PlatformStyle::kIsOkButtonLeading)
    std::swap(buttons[0], buttons[1]);

  return buttons;
}
