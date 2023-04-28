// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_controller.h"
#include <initializer_list>
#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_snooze_service.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_service.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

FeaturePromoController::FeaturePromoController() = default;
FeaturePromoController::~FeaturePromoController() = default;

FeaturePromoControllerCommon::FeaturePromoControllerCommon(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    FeaturePromoSnoozeService* snooze_service,
    TutorialService* tutorial_service)
    : registry_(registry),
      feature_engagement_tracker_(feature_engagement_tracker),
      bubble_factory_registry_(help_bubble_registry),
      snooze_service_(snooze_service),
      tutorial_service_(tutorial_service) {
  DCHECK(feature_engagement_tracker_);
  DCHECK(bubble_factory_registry_);
  DCHECK(snooze_service_);
}

FeaturePromoControllerCommon::~FeaturePromoControllerCommon() {
  // Inform any pending startup promos that they were not shown.
  for (auto& [feature, callback] : startup_promos_)
    std::move(callback).Run(*feature, false);
}

bool FeaturePromoControllerCommon::MaybeShowPromo(
    const base::Feature& iph_feature,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    BubbleCloseCallback close_callback) {
  // Fail if the promo is already queued to run at FE initialization.
  if (base::Contains(startup_promos_, &iph_feature))
    return false;

  const FeaturePromoSpecification* spec =
      registry()->GetParamsForFeature(iph_feature);
  if (!spec)
    return false;

  DCHECK_EQ(&iph_feature, spec->feature());
  DCHECK(spec->anchor_element_id());

  // Fetch the anchor element. For now, assume all elements are Views.
  ui::TrackedElement* const anchor_element =
      spec->GetAnchorElement(GetAnchorContext());

  if (!anchor_element)
    return false;

  return MaybeShowPromoFromSpecification(*spec, anchor_element,
                                         std::move(body_text_replacements),
                                         std::move(close_callback));
}

bool FeaturePromoControllerCommon::MaybeShowStartupPromo(
    const base::Feature& iph_feature,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    StartupPromoCallback promo_callback,
    BubbleCloseCallback close_callback) {
  // If the promo is currently running, fail.
  if (current_iph_feature_ == &iph_feature)
    return false;

  // If the promo is already queued, fail.
  if (base::Contains(startup_promos_, &iph_feature))
    return false;

  // Queue the promo.
  startup_promos_.emplace(&iph_feature, std::move(promo_callback));
  feature_engagement_tracker_->AddOnInitializedCallback(base::BindOnce(
      &FeaturePromoControllerCommon::OnFeatureEngagementTrackerInitialized,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(&iph_feature),
      std::move(body_text_replacements), std::move(close_callback)));

  // The promo has been successfully queued. Once the FE backend is initialized,
  // MaybeShowPromo() will be called to see if the promo should actually be
  // shown.
  return true;
}

bool FeaturePromoControllerCommon::MaybeShowPromoForDemoPage(
    const base::Feature* iph_feature,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    BubbleCloseCallback close_callback) {
  if (current_iph_feature_ && promo_bubble_)
    EndPromo(*current_iph_feature_);
  iph_feature_bypassing_tracker_ = iph_feature;

  bool showed_promo = MaybeShowPromo(*iph_feature);

  if (!showed_promo && iph_feature_bypassing_tracker_)
    iph_feature_bypassing_tracker_ = nullptr;

  return showed_promo;
}

bool FeaturePromoControllerCommon::MaybeShowPromoFromSpecification(
    const FeaturePromoSpecification& spec,
    ui::TrackedElement* anchor_element,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    BubbleCloseCallback close_callback) {
  CHECK(anchor_element);

  if (promos_blocked_for_testing_)
    return false;

  // A normal promo cannot show if a critical promo is displayed. These
  // are not registered with |tracker_| so check here.
  if (critical_promo_bubble_)
    return false;

  // Some contexts and anchors are not appropriate for showing normal promos.
  if (!CanShowPromo(anchor_element))
    return false;

  // Some checks should not be done in demo mode, because we absolutely want to
  // trigger the bubble if possible. Put any checks that should be bypassed in
  // demo mode in this block.
  const base::Feature* feature = spec.feature();
  bool feature_is_bypassing_tracker = feature == iph_feature_bypassing_tracker_;
  if (!(base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode) ||
        feature_is_bypassing_tracker) &&
      snooze_service_->IsBlocked(*feature))
    return false;

  // Can't show a standard promo if another help bubble is visible.
  if (bubble_factory_registry_->is_any_bubble_showing())
    return false;

  // TODO(crbug.com/1258216): Currently this must be called before
  // ShouldTriggerHelpUI() below. See bug for details.
  const bool screen_reader_available = CheckScreenReaderPromptAvailable();

  if (!feature_is_bypassing_tracker &&
      !feature_engagement_tracker_->ShouldTriggerHelpUI(*feature))
    return false;

  // If the tracker says we should trigger, but we have a promo
  // currently showing, there is a bug somewhere in here.
  DCHECK(!current_iph_feature_);
  current_iph_feature_ = feature;

  // Try to show the bubble and bail out if we cannot.
  promo_bubble_ = ShowPromoBubbleImpl(
      spec, anchor_element, std::move(body_text_replacements),
      screen_reader_available, /* is_critical_promo =*/false);
  if (!promo_bubble_) {
    current_iph_feature_ = nullptr;
    if (!feature_is_bypassing_tracker)
      feature_engagement_tracker_->Dismissed(*feature);
    return false;
  }

  bubble_closed_callback_ = std::move(close_callback);

  if (!feature_is_bypassing_tracker)
    snooze_service_->OnPromoShown(*feature);

  return true;
}

std::unique_ptr<HelpBubble> FeaturePromoControllerCommon::ShowCriticalPromo(
    const FeaturePromoSpecification& spec,
    ui::TrackedElement* anchor_element,
    FeaturePromoSpecification::StringReplacements body_text_replacements) {
  if (promos_blocked_for_testing_)
    return nullptr;

  // Don't preempt an existing critical promo.
  if (critical_promo_bubble_)
    return nullptr;

  // If a normal bubble is showing, close it. Won't affect a promo continued
  // after its bubble has closed.
  if (current_iph_feature_)
    EndPromo(*current_iph_feature_);

  // Snooze and tutorial are not supported for critical promos.
  DCHECK_NE(FeaturePromoSpecification::PromoType::kSnooze, spec.promo_type());
  DCHECK_NE(FeaturePromoSpecification::PromoType::kTutorial, spec.promo_type());

  // Create the bubble.
  auto bubble = ShowPromoBubbleImpl(
      spec, anchor_element, std::move(body_text_replacements),
      CheckScreenReaderPromptAvailable(), /* is_critical_promo =*/true);
  critical_promo_bubble_ = bubble.get();
  return bubble;
}

FeaturePromoStatus FeaturePromoControllerCommon::GetPromoStatus(
    const base::Feature& iph_feature) const {
  if (base::Contains(startup_promos_, &iph_feature))
    return FeaturePromoStatus::kQueuedForStartup;
  if (current_iph_feature_ != &iph_feature)
    return FeaturePromoStatus::kNotRunning;
  return (promo_bubble_ && promo_bubble_->is_open())
             ? FeaturePromoStatus::kBubbleShowing
             : FeaturePromoStatus::kContinued;
}

bool FeaturePromoControllerCommon::EndPromo(const base::Feature& iph_feature) {
  const auto it = startup_promos_.find(&iph_feature);
  if (it != startup_promos_.end()) {
    std::move(it->second).Run(iph_feature, false);
    startup_promos_.erase(it);
    return true;
  }

  if (current_iph_feature_ != &iph_feature)
    return false;

  const bool was_open = promo_bubble_ && promo_bubble_->is_open();
  if (promo_bubble_)
    promo_bubble_->Close();
  if (!continuing_after_bubble_closed_ &&
      iph_feature_bypassing_tracker_ == &iph_feature) {
    iph_feature_bypassing_tracker_ = nullptr;
  }
  return was_open;
}

bool FeaturePromoControllerCommon::DismissNonCriticalBubbleInRegion(
    const gfx::Rect& screen_bounds) {
  if (promo_bubble_ && promo_bubble_->is_open() &&
      promo_bubble_->GetBoundsInScreen().Intersects(screen_bounds)) {
    const bool result = EndPromo(*current_iph_feature_);
    DCHECK(result);
    return result;
  }
  return false;
}

FeaturePromoHandle FeaturePromoControllerCommon::CloseBubbleAndContinuePromo(
    const base::Feature& iph_feature) {
  DCHECK_EQ(current_iph_feature_, &iph_feature);
  continuing_after_bubble_closed_ = true;
  const bool result = EndPromo(iph_feature);
  DCHECK(result);
  return FeaturePromoHandle(GetAsWeakPtr(), &iph_feature);
}

base::WeakPtr<FeaturePromoController>
FeaturePromoControllerCommon::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

FeaturePromoControllerCommon::TestLock
FeaturePromoControllerCommon::BlockPromosForTesting() {
  if (current_iph_feature_)
    EndPromo(*current_iph_feature_);
  return std::make_unique<base::AutoReset<bool>>(&promos_blocked_for_testing_,
                                                 true);
}

bool FeaturePromoControllerCommon::CheckScreenReaderPromptAvailable() const {
  if (!ui::AXPlatformNode::GetAccessibilityMode().has_mode(
          ui::AXMode::kScreenReader)) {
    return false;
  }

  // If we're in demo mode and screen reader is on, always play the demo
  // without querying the FE backend, since the backend will return false for
  // all promos other than the one that's being demoed. If we didn't have this
  // code the screen reader prompt would never play.
  if (base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode) ||
      iph_feature_bypassing_tracker_)
    return true;

  const base::Feature* const prompt_feature =
      GetScreenReaderPromptPromoFeature();
  if (!prompt_feature ||
      !feature_engagement_tracker_->ShouldTriggerHelpUI(*prompt_feature))
    return false;

  // TODO(crbug.com/1258216): Once we have our answer, immediately dismiss
  // so that this doesn't interfere with actually showing the bubble. This
  // dismiss can be moved elsewhere once we support concurrency.
  feature_engagement_tracker_->Dismissed(*prompt_feature);

  return true;
}

void FeaturePromoControllerCommon::OnFeatureEngagementTrackerInitialized(
    const base::Feature* iph_feature,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    BubbleCloseCallback close_callback,
    bool tracker_initialized_successfully) {
  // If the promo has been canceled, do not proceed.
  const auto it = startup_promos_.find(iph_feature);
  if (it == startup_promos_.end())
    return;

  // Store the callback and remove the promo from the pending list.
  StartupPromoCallback callback = std::move(it->second);
  startup_promos_.erase(it);

  // Try to start the promo, assuming the tracker was successfully initialized.
  bool success = false;
  if (tracker_initialized_successfully) {
    success = MaybeShowPromo(*iph_feature, std::move(body_text_replacements),
                             std::move(close_callback));
  }
  std::move(callback).Run(*iph_feature, success);
}

std::unique_ptr<HelpBubble> FeaturePromoControllerCommon::ShowPromoBubbleImpl(
    const FeaturePromoSpecification& spec,
    ui::TrackedElement* anchor_element,
    FeaturePromoSpecification::StringReplacements body_text_replacements,
    bool screen_reader_prompt_available,
    bool is_critical_promo) {
  HelpBubbleParams create_params;
  create_params.body_text = l10n_util::GetStringFUTF16(
      spec.bubble_body_string_id(), std::move(body_text_replacements), nullptr);
  create_params.title_text = spec.bubble_title_text();
  if (spec.screen_reader_string_id()) {
    create_params.screenreader_text =
        spec.screen_reader_accelerator()
            ? l10n_util::GetStringFUTF16(
                  spec.screen_reader_string_id(),
                  spec.screen_reader_accelerator()
                      .GetAccelerator(GetAcceleratorProvider())
                      .GetShortcutText())
            : l10n_util::GetStringUTF16(spec.screen_reader_string_id());
  }
  create_params.close_button_alt_text =
      l10n_util::GetStringUTF16(IDS_CLOSE_PROMO);
  create_params.body_icon = spec.bubble_icon();
  if (spec.bubble_body_string_id())
    create_params.body_icon_alt_text = GetBodyIconAltText();
  create_params.arrow = spec.bubble_arrow();

  // Critical promos don't time out.
  if (is_critical_promo)
    create_params.timeout = base::Seconds(0);

  // Feature isn't present for some critical promos.
  if (spec.feature()) {
    create_params.dismiss_callback = base::BindOnce(
        &FeaturePromoControllerCommon::OnHelpBubbleDismissed,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()));
  }

  switch (spec.promo_type()) {
    case FeaturePromoSpecification::PromoType::kSnooze:
      CHECK(spec.feature());
      create_params.buttons = CreateSnoozeButtons(*spec.feature());
      break;
    case FeaturePromoSpecification::PromoType::kTutorial:
      CHECK(spec.feature());
      create_params.buttons =
          CreateTutorialButtons(*spec.feature(), spec.tutorial_id());
      create_params.dismiss_callback = base::BindOnce(
          &FeaturePromoControllerCommon::OnTutorialHelpBubbleDismissed,
          weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()),
          spec.tutorial_id());
      break;
    case FeaturePromoSpecification::PromoType::kCustomAction:
      CHECK(spec.feature());
      create_params.buttons = CreateCustomActionButtons(
          *spec.feature(), spec.custom_action_caption(),
          spec.custom_action_callback(), spec.custom_action_is_default(),
          spec.custom_action_dismiss_string_id());
      break;
    case FeaturePromoSpecification::PromoType::kUnspecified:
    case FeaturePromoSpecification::PromoType::kToast:
    case FeaturePromoSpecification::PromoType::kLegacy:
      break;
  }

  bool had_screen_reader_promo = false;
  if (spec.promo_type() == FeaturePromoSpecification::PromoType::kTutorial) {
    create_params.keyboard_navigation_hint = GetTutorialScreenReaderHint();
  } else if (CheckScreenReaderPromptAvailable()) {
    create_params.keyboard_navigation_hint = GetFocusHelpBubbleScreenReaderHint(
        spec.promo_type(), anchor_element, is_critical_promo);
    had_screen_reader_promo = !create_params.keyboard_navigation_hint.empty();
  }

  auto help_bubble = bubble_factory_registry_->CreateHelpBubble(
      anchor_element, std::move(create_params));
  if (help_bubble) {
    // Record that the focus help message was actually read to the user. See the
    // note in MaybeShowPromoImpl().
    // TODO(crbug.com/1258216): Rewrite this when we have the ability for FE
    // promos to ignore other active promos.
    if (had_screen_reader_promo) {
      feature_engagement_tracker_->NotifyEvent(
          GetScreenReaderPromptPromoEventName());
    }

    // Listen for the bubble being closed.
    bubble_closed_subscription_ = help_bubble->AddOnCloseCallback(
        base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleClosed,
                       base::Unretained(this)));
  }

  return help_bubble;
}

void FeaturePromoControllerCommon::FinishContinuedPromo(
    const base::Feature& iph_feature) {
  DCHECK(continuing_after_bubble_closed_);
  if (iph_feature_bypassing_tracker_ != &iph_feature)
    feature_engagement_tracker_->Dismissed(iph_feature);
  else
    iph_feature_bypassing_tracker_ = nullptr;
  if (current_iph_feature_ == &iph_feature) {
    current_iph_feature_ = nullptr;
    continuing_after_bubble_closed_ = false;
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleClosed(HelpBubble* bubble) {
  // Since we're in the middle of processing callbacks we can't reset our
  // subscription but since it's a weak pointer (internally) and since we should
  // should only get called here once, it's not a big deal if we don't reset
  // it.
  if (bubble == critical_promo_bubble_) {
    critical_promo_bubble_ = nullptr;
  } else if (bubble == promo_bubble_.get()) {
    if (!continuing_after_bubble_closed_) {
      if (iph_feature_bypassing_tracker_.get() != current_iph_feature_)
        feature_engagement_tracker_->Dismissed(*current_iph_feature_);
      else
        iph_feature_bypassing_tracker_ = nullptr;
      current_iph_feature_ = nullptr;
    }
    promo_bubble_.reset();
  } else {
    NOTREACHED();
  }

  if (bubble_closed_callback_)
    std::move(bubble_closed_callback_).Run();
}

void FeaturePromoControllerCommon::OnHelpBubbleSnoozed(
    const base::Feature* feature) {
  if (iph_feature_bypassing_tracker_ != feature)
    snooze_service_->OnUserSnooze(*feature);
}

void FeaturePromoControllerCommon::OnHelpBubbleDismissed(
    const base::Feature* feature) {
  if (snooze_service_ && iph_feature_bypassing_tracker_ != feature)
    snooze_service_->OnUserDismiss(*feature);
}

void FeaturePromoControllerCommon::OnCustomAction(
    const base::Feature* feature,
    FeaturePromoSpecification::CustomActionCallback callback) {
  callback.Run(GetAnchorContext(), CloseBubbleAndContinuePromo(*feature));
}

void FeaturePromoControllerCommon::OnTutorialHelpBubbleSnoozed(
    const base::Feature* iph_feature,
    TutorialIdentifier tutorial_id) {
  OnHelpBubbleSnoozed(iph_feature);
  tutorial_service_->LogIPHLinkClicked(tutorial_id, false);
}

void FeaturePromoControllerCommon::OnTutorialHelpBubbleDismissed(
    const base::Feature* iph_feature,
    TutorialIdentifier tutorial_id) {
  OnHelpBubbleDismissed(iph_feature);
  tutorial_service_->LogIPHLinkClicked(tutorial_id, false);
}

void FeaturePromoControllerCommon::OnTutorialStarted(
    const base::Feature* iph_feature,
    TutorialIdentifier tutorial_id) {
  if (!promo_bubble_ || !promo_bubble_->is_open()) {
    NOTREACHED();
  } else {
    DCHECK_EQ(current_iph_feature_, iph_feature);
    tutorial_promo_handle_ = CloseBubbleAndContinuePromo(*iph_feature);
    DCHECK(tutorial_promo_handle_.is_valid());
    tutorial_service_->StartTutorial(
        tutorial_id, GetAnchorContext(),
        base::BindOnce(&FeaturePromoControllerCommon::OnTutorialComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(iph_feature)),
        base::BindOnce(&FeaturePromoControllerCommon::OnTutorialAborted,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(iph_feature)));
    if (tutorial_service_->IsRunningTutorial()) {
      tutorial_service_->LogIPHLinkClicked(tutorial_id, true);
    }
  }
}

void FeaturePromoControllerCommon::OnTutorialComplete(
    const base::Feature* iph_feature) {
  tutorial_promo_handle_.Release();
  if (snooze_service_)
    snooze_service_->OnUserDismiss(*iph_feature);
}

void FeaturePromoControllerCommon::OnTutorialAborted(
    const base::Feature* iph_feature) {
  tutorial_promo_handle_.Release();
  if (snooze_service_)
    snooze_service_->OnUserSnooze(*iph_feature);
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerCommon::CreateSnoozeButtons(
    const base::Feature& feature) {
  std::vector<HelpBubbleButtonParams> buttons;

  HelpBubbleButtonParams snooze_button;
  snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
  snooze_button.is_default = false;
  snooze_button.callback = base::BindOnce(
      &FeaturePromoControllerCommon::OnHelpBubbleSnoozed,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature));
  buttons.push_back(std::move(snooze_button));

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
  dismiss_button.is_default = true;
  dismiss_button.callback = base::BindOnce(
      &FeaturePromoControllerCommon::OnHelpBubbleDismissed,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature));
  buttons.push_back(std::move(dismiss_button));

  return buttons;
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerCommon::CreateCustomActionButtons(
    const base::Feature& feature,
    const std::u16string& custom_action_caption,
    FeaturePromoSpecification::CustomActionCallback custom_action_callback,
    bool custom_action_is_default,
    int custom_action_dismiss_string_id) {
  std::vector<HelpBubbleButtonParams> buttons;
  CHECK(!custom_action_callback.is_null());

  HelpBubbleButtonParams action_button;
  action_button.text = custom_action_caption;
  action_button.is_default = custom_action_is_default;
  action_button.callback =
      base::BindOnce(&FeaturePromoControllerCommon::OnCustomAction,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature),
                     custom_action_callback);
  buttons.push_back(std::move(action_button));

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text =
      l10n_util::GetStringUTF16(custom_action_dismiss_string_id);
  dismiss_button.is_default = !custom_action_is_default;
  dismiss_button.callback = base::BindOnce(
      &FeaturePromoControllerCommon::OnHelpBubbleDismissed,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature));
  buttons.push_back(std::move(dismiss_button));

  return buttons;
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerCommon::CreateTutorialButtons(
    const base::Feature& feature,
    TutorialIdentifier tutorial_id) {
  std::vector<HelpBubbleButtonParams> buttons;

  HelpBubbleButtonParams snooze_button;
  snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
  snooze_button.is_default = false;
  snooze_button.callback = base::BindRepeating(
      &FeaturePromoControllerCommon::OnTutorialHelpBubbleSnoozed,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature), tutorial_id);
  buttons.push_back(std::move(snooze_button));

  HelpBubbleButtonParams tutorial_button;
  tutorial_button.text =
      l10n_util::GetStringUTF16(IDS_PROMO_SHOW_TUTORIAL_BUTTON);
  tutorial_button.is_default = true;
  tutorial_button.callback = base::BindRepeating(
      &FeaturePromoControllerCommon::OnTutorialStarted,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature), tutorial_id);
  buttons.push_back(std::move(tutorial_button));

  return buttons;
}

// static
bool FeaturePromoControllerCommon::active_window_check_blocked_ = false;

// static
FeaturePromoControllerCommon::TestLock
FeaturePromoControllerCommon::BlockActiveWindowCheckForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&active_window_check_blocked_,
                                                 true);
}

}  // namespace user_education
