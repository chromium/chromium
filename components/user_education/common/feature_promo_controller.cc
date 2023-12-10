// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_controller.h"

#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
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

struct FeaturePromoControllerCommon::ShowPromoBubbleParams {
  ShowPromoBubbleParams() = default;
  ShowPromoBubbleParams(ShowPromoBubbleParams&& other) noexcept = default;
  ~ShowPromoBubbleParams() = default;

  raw_ptr<const FeaturePromoSpecification> spec = nullptr;
  raw_ptr<ui::TrackedElement> anchor_element = nullptr;
  FeaturePromoSpecification::FormatParameters body_format;
  FeaturePromoSpecification::FormatParameters title_format;
  bool screen_reader_prompt_available = false;
  bool can_snooze = false;
  bool is_critical_promo = false;
};

FeaturePromoControllerCommon::FeaturePromoControllerCommon(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    FeaturePromoStorageService* storage_service,
    FeaturePromoSessionPolicy* session_policy,
    TutorialService* tutorial_service)
    : in_iph_demo_mode_(
          base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode)),
      registry_(registry),
      feature_engagement_tracker_(feature_engagement_tracker),
      bubble_factory_registry_(help_bubble_registry),
      storage_service_(storage_service),
      session_policy_(session_policy),
      tutorial_service_(tutorial_service) {
  DCHECK(feature_engagement_tracker_);
  DCHECK(bubble_factory_registry_);
  DCHECK(storage_service_);
}

FeaturePromoControllerCommon::~FeaturePromoControllerCommon() {
  // Inform any pending startup promos that they were not shown.
  for (auto& [feature, callback] : startup_promos_) {
    if (callback) {
      std::move(callback).Run(*feature, FeaturePromoResult::kCanceled);
    }
  }
}

FeaturePromoResult FeaturePromoControllerCommon::CanShowPromo(
    const base::Feature& iph_feature) const {
  auto result = CanShowPromoCommon(iph_feature, false);
  if (result && !feature_engagement_tracker_->WouldTriggerHelpUI(iph_feature)) {
    result = FeaturePromoResult::kBlockedByConfig;
  }
  return result;
}

FeaturePromoResult FeaturePromoControllerCommon::MaybeShowPromo(
    FeaturePromoParams params) {
  return MaybeShowPromoCommon(std::move(params), /* for_demo =*/false);
}

bool FeaturePromoControllerCommon::MaybeShowStartupPromo(
    FeaturePromoParams params) {
  const base::Feature* const iph_feature = &params.feature.get();

  // If the promo is currently running, fail.
  if (GetCurrentPromoFeature() == iph_feature) {
    return false;
  }

  // If the promo is already queued, fail.
  if (base::Contains(startup_promos_, iph_feature)) {
    return false;
  }

  // Queue the promo.
  startup_promos_.emplace(iph_feature, std::move(params.startup_callback));
  feature_engagement_tracker_->AddOnInitializedCallback(base::BindOnce(
      &FeaturePromoControllerCommon::OnFeatureEngagementTrackerInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(params)));

  // The promo has been successfully queued. Once the FE backend is initialized,
  // MaybeShowPromo() will be called to see if the promo should actually be
  // shown.
  return true;
}

FeaturePromoResult FeaturePromoControllerCommon::MaybeShowPromoForDemoPage(
    FeaturePromoParams params) {
  return MaybeShowPromoCommon(std::move(params), /* for_demo =*/true);
}

FeaturePromoResult FeaturePromoControllerCommon::MaybeShowPromoCommon(
    FeaturePromoParams params,
    bool for_demo) {
  // Perform common checks.
  const FeaturePromoSpecification* spec = nullptr;
  std::unique_ptr<FeaturePromoLifecycle> lifecycle = nullptr;
  ui::TrackedElement* anchor_element = nullptr;
  auto result = CanShowPromoCommon(params.feature.get(), for_demo, &spec,
                                   &lifecycle, &anchor_element);
  if (!result) {
    return result;
  }
  CHECK(spec);
  CHECK(lifecycle);
  CHECK(anchor_element);

  // If the session policy allows overriding the current promo, abort it.
  if (current_promo_) {
    EndPromo(*GetCurrentPromoFeature(),
             FeaturePromoClosedReason::kOverrideForDemo);
  }

  // If the session policy allows overriding other help bubbles, close them.
  if (auto* const help_bubble =
          bubble_factory_registry_->GetHelpBubble(anchor_element->context())) {
    help_bubble->Close();
  }

  // TODO(crbug.com/1258216): Currently this must be called before
  // ShouldTriggerHelpUI() below. See bug for details.
  const bool screen_reader_available =
      CheckScreenReaderPromptAvailable(for_demo || in_iph_demo_mode_);

  if (!for_demo &&
      !feature_engagement_tracker_->ShouldTriggerHelpUI(params.feature.get())) {
    return FeaturePromoResult::kBlockedByConfig;
  }

  // If the tracker says we should trigger, but we have a promo
  // currently showing, there is a bug somewhere in here.
  DCHECK(!current_promo_);
  current_promo_ = std::move(lifecycle);

  // Construct the parameters for the promotion.
  ShowPromoBubbleParams show_params;
  show_params.spec = spec;
  show_params.anchor_element = anchor_element;
  show_params.screen_reader_prompt_available = screen_reader_available;
  show_params.body_format = std::move(params.body_params);
  show_params.title_format = std::move(params.title_params);
  show_params.can_snooze = current_promo_->CanSnooze();

  // Try to show the bubble and bail out if we cannot.
  auto bubble = ShowPromoBubbleImpl(std::move(show_params));
  if (!bubble) {
    current_promo_.reset();
    if (!for_demo) {
      feature_engagement_tracker_->Dismissed(params.feature.get());
    }
    return FeaturePromoResult::kError;
  }

  // Update the most recent promo info.
  last_promo_info_ = session_policy_->SpecificationToPromoInfo(*spec);
  session_policy_->NotifyPromoShown(last_promo_info_);

  bubble_closed_callback_ = std::move(params.close_callback);

  if (for_demo) {
    current_promo_->OnPromoShownForDemo(std::move(bubble));
  } else {
    current_promo_->OnPromoShown(std::move(bubble),
                                 feature_engagement_tracker_);
  }

  return result;
}

std::unique_ptr<HelpBubble> FeaturePromoControllerCommon::ShowCriticalPromo(
    const FeaturePromoSpecification& spec,
    ui::TrackedElement* anchor_element,
    FeaturePromoSpecification::FormatParameters body_params,
    FeaturePromoSpecification::FormatParameters title_params) {
  // Don't preempt an existing critical promo.
  if (critical_promo_bubble_)
    return nullptr;

  // If a normal bubble is showing, close it. Won't affect a promo continued
  // after its bubble has closed.
  if (const auto* current = GetCurrentPromoFeature()) {
    EndPromo(*current, FeaturePromoClosedReason::kOverrideForPrecedence);
  }

  // Snooze and tutorial are not supported for critical promos.
  DCHECK_NE(FeaturePromoSpecification::PromoType::kSnooze, spec.promo_type());
  DCHECK_NE(FeaturePromoSpecification::PromoType::kTutorial, spec.promo_type());

  ShowPromoBubbleParams show_params;
  show_params.spec = &spec;
  show_params.anchor_element = anchor_element;
  show_params.body_format = std::move(body_params);
  show_params.title_format = std::move(title_params);
  show_params.screen_reader_prompt_available =
      CheckScreenReaderPromptAvailable(/* for_demo =*/false);
  show_params.is_critical_promo = true;

  auto bubble = ShowPromoBubbleImpl(std::move(show_params));
  critical_promo_bubble_ = bubble.get();

  // Update the most recent promo info. Critical promos are always high
  // priority.
  // TODO(dfried): we should probably verify that the bubble succeeded?
  last_promo_info_ = session_policy_->SpecificationToPromoInfo(spec);
  last_promo_info_.priority = FeaturePromoSessionPolicy::PromoPriority::kHigh;
  session_policy_->NotifyPromoShown(last_promo_info_);

  return bubble;
}

FeaturePromoStatus FeaturePromoControllerCommon::GetPromoStatus(
    const base::Feature& iph_feature) const {
  if (base::Contains(startup_promos_, &iph_feature))
    return FeaturePromoStatus::kQueuedForStartup;
  if (GetCurrentPromoFeature() != &iph_feature) {
    return FeaturePromoStatus::kNotRunning;
  }
  return current_promo_->is_bubble_visible()
             ? FeaturePromoStatus::kBubbleShowing
             : FeaturePromoStatus::kContinued;
}

const FeaturePromoSpecification*
FeaturePromoControllerCommon::GetCurrentPromoSpecificationForAnchor(
    ui::ElementIdentifier menu_element_id) const {
  auto* iph_feature = current_promo_ ? current_promo_->iph_feature() : nullptr;
  if (iph_feature && registry_) {
    auto* spec = registry_->GetParamsForFeature(*iph_feature);
    if (spec->anchor_element_id() == menu_element_id) {
      return spec;
    }
  }
  return {};
}

bool FeaturePromoControllerCommon::HasPromoBeenDismissed(
    const base::Feature& iph_feature,
    FeaturePromoClosedReason* last_close_reason) const {
  const FeaturePromoSpecification* spec =
      registry()->GetParamsForFeature(iph_feature);
  if (!spec) {
    return false;
  }

  const auto data = storage_service()->ReadPromoData(iph_feature);
  if (!data) {
    return false;
  }

  if (last_close_reason) {
    *last_close_reason = data->last_dismissed_by;
  }

  switch (spec->promo_subtype()) {
    case user_education::FeaturePromoSpecification::PromoSubtype::kNormal:
    case user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice:
      return data->is_dismissed;
    case user_education::FeaturePromoSpecification::PromoSubtype::kPerApp:
      return base::Contains(data->shown_for_apps, GetAppId());
  }
}

bool FeaturePromoControllerCommon::EndPromo(
    const base::Feature& iph_feature,
    EndFeaturePromoReason end_promo_reason) {
  // Translate public enum UserCloseReason to private
  // UserCloseReasonInternal and call private method.
  auto close_reason_internal =
      end_promo_reason == EndFeaturePromoReason::kFeatureEngaged
          ? FeaturePromoClosedReason::kFeatureEngaged
          : FeaturePromoClosedReason::kAbortPromo;
  return EndPromo(iph_feature, close_reason_internal);
}

bool FeaturePromoControllerCommon::EndPromo(
    const base::Feature& iph_feature,
    FeaturePromoClosedReason close_reason) {
  const auto it = startup_promos_.find(&iph_feature);
  if (it != startup_promos_.end()) {
    if (it->second) {
      std::move(it->second).Run(iph_feature, FeaturePromoResult::kCanceled);
    }
    startup_promos_.erase(it);
    return true;
  }

  if (GetCurrentPromoFeature() != &iph_feature) {
    return false;
  }

  const bool was_open = current_promo_->is_bubble_visible();
  RecordPromoEnded(close_reason, /*continue_after_close=*/false);
  return was_open;
}

void FeaturePromoControllerCommon::RecordPromoEnded(
    FeaturePromoClosedReason close_reason,
    bool continue_after_close) {
  session_policy_->NotifyPromoEnded(last_promo_info_, close_reason);
  current_promo_->OnPromoEnded(close_reason, continue_after_close);
  if (!continue_after_close) {
    current_promo_.reset();
  }
}

bool FeaturePromoControllerCommon::DismissNonCriticalBubbleInRegion(
    const gfx::Rect& screen_bounds) {
  const auto* const bubble = promo_bubble();
  if (!bubble || !bubble->is_open() ||
      !bubble->GetBoundsInScreen().Intersects(screen_bounds)) {
    return false;
  }
  const bool result =
      EndPromo(*current_promo_->iph_feature(),
               FeaturePromoClosedReason::kOverrideForUIRegionConflict);
  DCHECK(result);
  return result;
}

FeaturePromoHandle FeaturePromoControllerCommon::CloseBubbleAndContinuePromo(
    const base::Feature& iph_feature) {
  return CloseBubbleAndContinuePromoWithReason(
      iph_feature, FeaturePromoClosedReason::kFeatureEngaged);
}

FeaturePromoHandle
FeaturePromoControllerCommon::CloseBubbleAndContinuePromoWithReason(
    const base::Feature& iph_feature,
    FeaturePromoClosedReason close_reason) {
  DCHECK_EQ(GetCurrentPromoFeature(), &iph_feature);
  RecordPromoEnded(close_reason, /*continue_after_close=*/true);
  return FeaturePromoHandle(GetAsWeakPtr(), &iph_feature);
}

base::WeakPtr<FeaturePromoController>
FeaturePromoControllerCommon::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FeaturePromoControllerCommon::CheckScreenReaderPromptAvailable(
    bool for_demo) const {
  if (!ui::AXPlatformNode::GetAccessibilityMode().has_mode(
          ui::AXMode::kScreenReader)) {
    return false;
  }

  // If we're in demo mode and screen reader is on, always play the demo
  // without querying the FE backend, since the backend will return false for
  // all promos other than the one that's being demoed. If we didn't have this
  // code the screen reader prompt would never play.
  if (for_demo) {
    return true;
  }

  const base::Feature* const prompt_feature =
      GetScreenReaderPromptPromoFeature();
  if (!prompt_feature ||
      !feature_engagement_tracker_->ShouldTriggerHelpUI(*prompt_feature)) {
    return false;
  }

  // TODO(crbug.com/1258216): Once we have our answer, immediately dismiss
  // so that this doesn't interfere with actually showing the bubble. This
  // dismiss can be moved elsewhere once we support concurrency.
  feature_engagement_tracker_->Dismissed(*prompt_feature);

  return true;
}

void FeaturePromoControllerCommon::OnFeatureEngagementTrackerInitialized(
    FeaturePromoParams params,
    bool tracker_initialized_successfully) {
  const base::Feature* const iph_feature = &params.feature.get();

  // If the promo has been canceled, do not proceed.
  const auto it = startup_promos_.find(iph_feature);
  if (it == startup_promos_.end()) {
    return;
  }

  // Store the callback and remove the promo from the pending list.
  StartupPromoCallback callback = std::move(it->second);
  startup_promos_.erase(it);

  // Try to start the promo, assuming the tracker was successfully initialized.
  FeaturePromoResult result;
  if (tracker_initialized_successfully) {
    result = MaybeShowPromo(std::move(params));
  } else {
    result = FeaturePromoResult::kError;
  }
  if (callback) {
    std::move(callback).Run(*iph_feature, result);
  }
}

FeaturePromoResult FeaturePromoControllerCommon::CanShowPromoCommon(
    const base::Feature& iph_feature,
    bool for_demo,
    const FeaturePromoSpecification** spec_out,
    std::unique_ptr<FeaturePromoLifecycle>* lifecycle_out,
    ui::TrackedElement** anchor_element_out) const {
  // Ensure that this promo isn't already queued for startup.
  //
  // Note that this check is bypassed if this is for an explicit demo, but not
  // in demo mode, as the IPH may be queued for startup specifically because it
  // is being demoed.
  if (!for_demo && base::Contains(startup_promos_, &iph_feature)) {
    return FeaturePromoResult::kBlockedByPromo;
  }

  const FeaturePromoSpecification* spec =
      registry()->GetParamsForFeature(iph_feature);
  if (!spec) {
    return FeaturePromoResult::kError;
  }

  // When not bypassing the normal gating systems, don't try to show promos for
  // disabled features. This prevents us from calling into the Feature
  // Engagement tracker more times than necessary, emitting unnecessary logging
  // events when features are disabled.
  if (!for_demo && !in_iph_demo_mode_ &&
      !base::FeatureList::IsEnabled(iph_feature)) {
    return FeaturePromoResult::kFeatureDisabled;
  }

  // Fetch the anchor element. For now, assume all elements are Views.
  ui::TrackedElement* const anchor_element =
      spec->GetAnchorElement(GetAnchorContext());
  if (!anchor_element) {
    return FeaturePromoResult::kBlockedByUi;
  }

  absl::optional<FeaturePromoSessionPolicy::PromoInfo> current_promo;
  if (critical_promo_bubble_ || current_promo_) {
    current_promo = last_promo_info_;
  } else if (bubble_factory_registry_->is_any_bubble_showing()) {
    current_promo = FeaturePromoSessionPolicy::PromoInfo();
  }

  // Defer to the session policy to determine if the promo can show.
  if (const auto result = session_policy_->CanShowPromo(
          session_policy_->SpecificationToPromoInfo(*spec), current_promo);
      !result) {
    return result;
  }

  // Some contexts and anchors are not appropriate for showing normal promos.
  if (!CanShowPromoForElement(anchor_element)) {
    return FeaturePromoResult::kBlockedByUi;
  }

  // Check the lifecycle, but only if not in demo mode. This is especially
  // important for snoozeable, app, and legal notice promos.
  if (!for_demo && !in_iph_demo_mode_) {
    auto lifecycle = std::make_unique<FeaturePromoLifecycle>(
        storage_service_, GetAppId(), &iph_feature, spec->promo_type(),
        spec->promo_subtype());
    if (const auto result = lifecycle->CanShow(); !result) {
      return result;
    }
    if (lifecycle_out) {
      *lifecycle_out = std::move(lifecycle);
    }
  } else if (lifecycle_out) {
    // If in demo mode but the caller has asked for a lifecycle anyway, then
    // provide one.
    *lifecycle_out = std::make_unique<FeaturePromoLifecycle>(
        storage_service_, GetAppId(), &iph_feature, spec->promo_type(),
        spec->promo_subtype());
  }

  // If the caller has asked for the specification or anchor element, then
  // provide them.
  if (spec_out) {
    *spec_out = spec;
  }
  if (anchor_element_out) {
    *anchor_element_out = anchor_element;
  }

  // Success - the promo can show.
  return FeaturePromoResult::Success();
}

std::unique_ptr<HelpBubble> FeaturePromoControllerCommon::ShowPromoBubbleImpl(
    ShowPromoBubbleParams params) {
  const auto& spec = *params.spec;
  HelpBubbleParams bubble_params;
  bubble_params.body_text = FeaturePromoSpecification::FormatString(
      spec.bubble_body_string_id(), std::move(params.body_format));
  bubble_params.title_text = FeaturePromoSpecification::FormatString(
      spec.bubble_title_string_id(), std::move(params.title_format));
  if (spec.screen_reader_string_id()) {
    bubble_params.screenreader_text =
        spec.screen_reader_accelerator()
            ? l10n_util::GetStringFUTF16(
                  spec.screen_reader_string_id(),
                  spec.screen_reader_accelerator()
                      .GetAccelerator(GetAcceleratorProvider())
                      .GetShortcutText())
            : l10n_util::GetStringUTF16(spec.screen_reader_string_id());
  }
  bubble_params.close_button_alt_text =
      l10n_util::GetStringUTF16(IDS_CLOSE_PROMO);
  bubble_params.body_icon = spec.bubble_icon();
  if (spec.bubble_body_string_id())
    bubble_params.body_icon_alt_text = GetBodyIconAltText();
  bubble_params.arrow = spec.bubble_arrow();
  bubble_params.focus_on_show_hint = spec.focus_on_show_override();

  // Critical promos don't time out.
  if (params.is_critical_promo) {
    bubble_params.timeout = base::Seconds(0);
  } else {
    bubble_params.timeout_callback = base::BindOnce(
        &FeaturePromoControllerCommon::OnHelpBubbleTimeout,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()));
  }

  // Feature isn't present for some critical promos.
  if (spec.feature()) {
    bubble_params.dismiss_callback = base::BindOnce(
        &FeaturePromoControllerCommon::OnHelpBubbleDismissed,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()),
        /* via_action_button =*/false);
  }

  switch (spec.promo_type()) {
    case FeaturePromoSpecification::PromoType::kSnooze:
      CHECK(spec.feature());
      bubble_params.buttons =
          CreateSnoozeButtons(*spec.feature(), params.can_snooze);
      break;
    case FeaturePromoSpecification::PromoType::kTutorial:
      CHECK(spec.feature());
      bubble_params.buttons = CreateTutorialButtons(
          *spec.feature(), params.can_snooze, spec.tutorial_id());
      bubble_params.dismiss_callback = base::BindOnce(
          &FeaturePromoControllerCommon::OnTutorialHelpBubbleDismissed,
          weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()),
          spec.tutorial_id());
      break;
    case FeaturePromoSpecification::PromoType::kCustomAction:
      CHECK(spec.feature());
      bubble_params.buttons = CreateCustomActionButtons(
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
    bubble_params.keyboard_navigation_hint = GetTutorialScreenReaderHint();
  } else if (params.screen_reader_prompt_available) {
    bubble_params.keyboard_navigation_hint = GetFocusHelpBubbleScreenReaderHint(
        spec.promo_type(), params.anchor_element, params.is_critical_promo);
    had_screen_reader_promo = !bubble_params.keyboard_navigation_hint.empty();
  }

  auto help_bubble = bubble_factory_registry_->CreateHelpBubble(
      params.anchor_element, std::move(bubble_params));
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
  if (GetCurrentPromoFeature() == &iph_feature) {
    current_promo_->OnContinuedPromoEnded(/*completed_successfully=*/true);
    current_promo_.reset();
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleClosed(HelpBubble* bubble) {
  // Since we're in the middle of processing callbacks we can't reset our
  // subscription but since it's a weak pointer (internally) and since we should
  // should only get called here once, it's not a big deal if we don't reset
  // it.
  if (bubble == critical_promo_bubble_) {
    critical_promo_bubble_ = nullptr;
  } else if (bubble == promo_bubble()) {
    if (current_promo_->OnPromoBubbleClosed()) {
      current_promo_.reset();
    }
  }

  if (bubble_closed_callback_) {
    std::move(bubble_closed_callback_).Run();
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleTimedOut(
    const base::Feature* feature) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(FeaturePromoClosedReason::kTimeout,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleSnoozed(
    const base::Feature* feature) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(FeaturePromoClosedReason::kSnooze,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleDismissed(
    const base::Feature* feature,
    bool via_action_button) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(via_action_button ? FeaturePromoClosedReason::kDismiss
                                       : FeaturePromoClosedReason::kCancel,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleTimeout(
    const base::Feature* feature) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(FeaturePromoClosedReason::kTimeout,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerCommon::OnCustomAction(
    const base::Feature* feature,
    FeaturePromoSpecification::CustomActionCallback callback) {
  callback.Run(GetAnchorContext(),
               CloseBubbleAndContinuePromoWithReason(
                   *feature, FeaturePromoClosedReason::kAction));
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
  OnHelpBubbleDismissed(iph_feature,
                        /* via_action_button =*/true);
  tutorial_service_->LogIPHLinkClicked(tutorial_id, false);
}

void FeaturePromoControllerCommon::OnTutorialStarted(
    const base::Feature* iph_feature,
    TutorialIdentifier tutorial_id) {
  DCHECK_EQ(GetCurrentPromoFeature(), iph_feature);
  tutorial_promo_handle_ = CloseBubbleAndContinuePromoWithReason(
      *iph_feature, FeaturePromoClosedReason::kAction);
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

void FeaturePromoControllerCommon::OnTutorialComplete(
    const base::Feature* iph_feature) {
  tutorial_promo_handle_.Release();
  if (GetCurrentPromoFeature() == iph_feature) {
    current_promo_->OnContinuedPromoEnded(/*completed_successfully=*/true);
    current_promo_.reset();
  }
}

void FeaturePromoControllerCommon::OnTutorialAborted(
    const base::Feature* iph_feature) {
  tutorial_promo_handle_.Release();
  if (GetCurrentPromoFeature() == iph_feature) {
    current_promo_->OnContinuedPromoEnded(/*completed_successfully=*/false);
    current_promo_.reset();
  }
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerCommon::CreateSnoozeButtons(const base::Feature& feature,
                                                  bool can_snooze) {
  std::vector<HelpBubbleButtonParams> buttons;

  if (can_snooze) {
    HelpBubbleButtonParams snooze_button;
    snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    snooze_button.is_default = false;
    snooze_button.callback = base::BindOnce(
        &FeaturePromoControllerCommon::OnHelpBubbleSnoozed,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature));
    buttons.push_back(std::move(snooze_button));
  }

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
  dismiss_button.is_default = true;
  dismiss_button.callback =
      base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleDismissed,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature),
                     /* via_action_button =*/true);
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
  dismiss_button.callback =
      base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleDismissed,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature),
                     /* via_action_button =*/true);
  buttons.push_back(std::move(dismiss_button));

  return buttons;
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerCommon::CreateTutorialButtons(
    const base::Feature& feature,
    bool can_snooze,
    TutorialIdentifier tutorial_id) {
  std::vector<HelpBubbleButtonParams> buttons;

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.is_default = false;
  if (can_snooze) {
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    dismiss_button.callback = base::BindRepeating(
        &FeaturePromoControllerCommon::OnTutorialHelpBubbleSnoozed,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature),
        tutorial_id);
  } else {
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
    dismiss_button.callback = base::BindRepeating(
        &FeaturePromoControllerCommon::OnTutorialHelpBubbleDismissed,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(&feature),
        tutorial_id);
  }
  buttons.push_back(std::move(dismiss_button));

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

const base::Feature* FeaturePromoControllerCommon::GetCurrentPromoFeature()
    const {
  return current_promo_ ? current_promo_->iph_feature() : nullptr;
}

// static
bool FeaturePromoControllerCommon::active_window_check_blocked_ = false;

// static
FeaturePromoControllerCommon::TestLock
FeaturePromoControllerCommon::BlockActiveWindowCheckForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&active_window_check_blocked_,
                                                 true);
}

FeaturePromoParams::FeaturePromoParams(const base::Feature& iph_feature)
    : feature(iph_feature) {}
FeaturePromoParams::FeaturePromoParams(FeaturePromoParams&& other) noexcept =
    default;
FeaturePromoParams::~FeaturePromoParams() = default;

}  // namespace user_education
