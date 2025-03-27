// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_controller.h"

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/tutorial/tutorial.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

FeaturePromoController::FeaturePromoController() = default;
FeaturePromoController::~FeaturePromoController() = default;

void FeaturePromoController::PostShowPromoResult(
    ShowPromoResultCallback callback,
    FeaturePromoResult result) {
  if (callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FeaturePromoController::ShowPromoResultCallback callback,
               FeaturePromoResult result) { std::move(callback).Run(result); },
            std::move(callback), result));
  }
}

FeaturePromoControllerCommon::FeaturePromoControllerCommon(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    UserEducationStorageService* storage_service,
    FeaturePromoSessionPolicy* session_policy,
    TutorialService* tutorial_service)
    : registry_(registry),
      feature_engagement_tracker_(feature_engagement_tracker),
      bubble_factory_registry_(help_bubble_registry),
      storage_service_(storage_service),
      session_policy_(session_policy),
      tutorial_service_(tutorial_service) {
  DCHECK(feature_engagement_tracker_);
  DCHECK(bubble_factory_registry_);
  DCHECK(storage_service_);
}

FeaturePromoControllerCommon::~FeaturePromoControllerCommon() = default;

FeaturePromoStatus FeaturePromoControllerCommon::GetPromoStatus(
    const base::Feature& iph_feature) const {
  if (IsPromoQueued(iph_feature)) {
    return FeaturePromoStatus::kQueued;
  }
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
    auto* const spec = registry_->GetParamsForFeature(*iph_feature);
    if (spec->anchor_element_id() == menu_element_id) {
      return spec;
    }
  }
  return {};
}

bool FeaturePromoControllerCommon::HasPromoBeenDismissed(
    const FeaturePromoParams& params,
    FeaturePromoClosedReason* last_close_reason) const {
  const FeaturePromoSpecification* const spec =
      registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    return false;
  }

  const auto data = storage_service()->ReadPromoData(*params.feature);
  if (!data) {
    return false;
  }

  if (last_close_reason) {
    *last_close_reason = data->last_dismissed_by;
  }

  switch (spec->promo_subtype()) {
    case user_education::FeaturePromoSpecification::PromoSubtype::kNormal:
    case user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice:
    case user_education::FeaturePromoSpecification::PromoSubtype::
        kActionableAlert:
      return data->is_dismissed;
    case user_education::FeaturePromoSpecification::PromoSubtype::kKeyedNotice:
      if (params.key.empty()) {
        return false;
      }
      return base::Contains(data->shown_for_keys, params.key);
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
          : FeaturePromoClosedReason::kAbortedByFeature;
  return EndPromo(iph_feature, close_reason_internal);
}

bool FeaturePromoControllerCommon::EndPromo(
    const base::Feature& iph_feature,
    FeaturePromoClosedReason close_reason) {
  if (MaybeUnqueuePromo(iph_feature)) {
    return true;
  }

  if (GetCurrentPromoFeature() != &iph_feature) {
    return false;
  }

  const bool was_open = current_promo_->is_bubble_visible();
  RecordPromoEnded(close_reason, /*continue_after_close=*/false);
  return was_open;
}

void FeaturePromoControllerCommon::CloseHelpBubbleIfPresent(
    ui::ElementContext context) {
  if (auto* const help_bubble =
          bubble_factory_registry()->GetHelpBubble(context)) {
    help_bubble->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
  }
}

void FeaturePromoControllerCommon::RecordPromoEnded(
    FeaturePromoClosedReason close_reason,
    bool continue_after_close) {
  session_policy_->NotifyPromoEnded(last_promo_info_, close_reason);
  current_promo_->OnPromoEnded(close_reason, continue_after_close);
  if (!continue_after_close) {
    current_promo_.reset();
    // Try to show the next queued promo (if any) but only if the current promo
    // was not ended by being overridden; in that case a different promo is
    // already trying to show.
    if (close_reason != FeaturePromoClosedReason::kOverrideForDemo &&
        close_reason != FeaturePromoClosedReason::kOverrideForPrecedence) {
      MaybeShowQueuedPromo();
    }
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

#if !BUILDFLAG(IS_ANDROID)
void FeaturePromoControllerCommon::NotifyFeatureUsedIfValid(
    const base::Feature& feature) {
  if (base::FeatureList::IsEnabled(feature) &&
      registry_->IsFeatureRegistered(feature)) {
    feature_engagement_tracker_->NotifyUsedEvent(feature);
  }
}
#endif

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

bool FeaturePromoControllerCommon::CheckExtendedPropertiesPromptAvailable(
    bool for_demo) const {
  if (!ui::AXPlatform::GetInstance().GetMode().has_mode(
          ui::AXMode::kExtendedProperties)) {
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

  // TODO(crbug.com/40200981): Once we have our answer, immediately dismiss
  // so that this doesn't interfere with actually showing the bubble. This
  // dismiss can be moved elsewhere once we support concurrency.
  feature_engagement_tracker_->Dismissed(*prompt_feature);

  return true;
}

std::unique_ptr<FeaturePromoLifecycle>
FeaturePromoControllerCommon::CreateLifecycleFor(
    const FeaturePromoSpecification& spec,
    const FeaturePromoParams& params) const {
  auto lifecycle = std::make_unique<FeaturePromoLifecycle>(
      storage_service(), params.key, &*params.feature, spec.promo_type(),
      spec.promo_subtype(), spec.rotating_promos().size());
  if (spec.reshow_delay()) {
    lifecycle->SetReshowPolicy(*spec.reshow_delay(), spec.max_show_count());
  }
  return lifecycle;
}

std::unique_ptr<HelpBubble> FeaturePromoControllerCommon::ShowPromoBubbleImpl(
    FeaturePromoSpecification::BuildHelpBubbleParams params) {
  const auto& spec = *params.spec;
  HelpBubbleParams bubble_params;

  bool had_screen_reader_promo = false;
  std::unique_ptr<HelpBubble> help_bubble;
  if (spec.promo_type() == FeaturePromoSpecification::PromoType::kCustomUi) {
    auto result = spec.BuildCustomHelpBubble(
        GetAnchorContext(), spec.bubble_arrow(), std::move(params));
    help_bubble = std::move(std::get<std::unique_ptr<HelpBubble>>(result));
    CHECK(help_bubble);
    auto* const ui = std::get<base::WeakPtr<CustomHelpBubbleUi>>(result).get();
    CHECK(ui);
    custom_ui_result_subscription_ =
        ui->AddUserActionCallback(base::BindRepeating(
            [](FeaturePromoControllerCommon* controller,
               const FeaturePromoSpecification* spec,
               CustomHelpBubbleUi::UserAction action) {
              switch (action) {
                case CustomHelpBubbleUi::UserAction::kCancel:
                  controller->OnHelpBubbleDismissed(spec->feature(), false);
                  break;
                case CustomHelpBubbleUi::UserAction::kDismiss:
                  controller->OnHelpBubbleDismissed(spec->feature(), true);
                  break;
                case CustomHelpBubbleUi::UserAction::kAction:
                  CHECK(!spec->custom_action_callback().is_null())
                      << "FeaturePromoSpecification for custom UI must "
                         "specify custom action callback if custom UI "
                         "generates `kAction` result type.";
                  controller->OnCustomAction(spec->feature(),
                                             spec->custom_action_callback());
                  break;
                case CustomHelpBubbleUi::UserAction::kSnooze:
                  controller->OnHelpBubbleSnoozed(spec->feature());
                  break;
              }
            },
            base::Unretained(this), base::Unretained(&spec)));
  } else {
    bubble_params.body_text = FeaturePromoSpecification::FormatString(
        spec.bubble_body_string_id(), std::move(params.body_format));
    bubble_params.title_text = FeaturePromoSpecification::FormatString(
        spec.bubble_title_string_id(), std::move(params.title_format));
    if (spec.screen_reader_accelerator()) {
      CHECK(spec.screen_reader_string_id());
      CHECK(std::holds_alternative<FeaturePromoSpecification::NoSubstitution>(
          params.screen_reader_format))
          << "Accelerator and substitution are not compatible for screen "
             "reader text.";
      bubble_params.screenreader_text = l10n_util::GetStringFUTF16(
          spec.screen_reader_string_id(),
          spec.screen_reader_accelerator()
              .GetAccelerator(GetAcceleratorProvider())
              .GetShortcutText());
    } else {
      bubble_params.screenreader_text = FeaturePromoSpecification::FormatString(
          spec.screen_reader_string_id(),
          std::move(params.screen_reader_format));
    }
    bubble_params.close_button_alt_text =
        l10n_util::GetStringUTF16(IDS_CLOSE_PROMO);
    bubble_params.body_icon = spec.bubble_icon();
    if (spec.bubble_body_string_id()) {
      bubble_params.body_icon_alt_text = GetBodyIconAltText();
    }
    bubble_params.arrow = spec.bubble_arrow();
    bubble_params.focus_on_show_hint = spec.focus_on_show_override();

    bubble_params.timeout_callback =
        base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleTimeout,
                       GetCommonWeakPtr(), base::Unretained(spec.feature()));

    // Feature isn't present for some critical promos.
    if (spec.feature()) {
      bubble_params.dismiss_callback =
          base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleDismissed,
                         GetCommonWeakPtr(), base::Unretained(spec.feature()),
                         /* via_action_button =*/false);
    }

    switch (spec.promo_type()) {
      case FeaturePromoSpecification::PromoType::kToast: {
        // Rotating toast promos require a "got it" button.
        if (current_promo_ &&
            current_promo_->promo_type() ==
                FeaturePromoSpecification::PromoType::kRotating) {
          bubble_params.buttons = CreateRotatingToastButtons(*spec.feature());
          // If no hint is set, promos with buttons take focus. However, toasts
          // do not take focus by default. So if the hint isn't already set, set
          // the promo not to take focus.
          bubble_params.focus_on_show_hint =
              bubble_params.focus_on_show_hint.value_or(false);
        }
        break;
      }
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
            GetCommonWeakPtr(), base::Unretained(spec.feature()),
            spec.tutorial_id());
        break;
      case FeaturePromoSpecification::PromoType::kCustomAction:
        CHECK(spec.feature());
        bubble_params.buttons = CreateCustomActionButtons(
            *spec.feature(), spec.custom_action_caption(),
            spec.custom_action_callback(), spec.custom_action_is_default(),
            spec.custom_action_dismiss_string_id());
        break;
      case FeaturePromoSpecification::PromoType::kCustomUi:
      case FeaturePromoSpecification::PromoType::kUnspecified:
      case FeaturePromoSpecification::PromoType::kLegacy:
        break;
      case FeaturePromoSpecification::PromoType::kRotating:
        NOTREACHED() << "Not implemented; should never reach this code.";
    }

    if (spec.promo_type() == FeaturePromoSpecification::PromoType::kTutorial) {
      bubble_params.keyboard_navigation_hint = GetTutorialScreenReaderHint();
    } else if (params.screen_reader_prompt_available) {
      bubble_params.keyboard_navigation_hint =
          GetFocusHelpBubbleScreenReaderHint(spec.promo_type(),
                                             params.anchor_element);
      had_screen_reader_promo = !bubble_params.keyboard_navigation_hint.empty();
    }
    help_bubble = bubble_factory_registry_->CreateHelpBubble(
        params.anchor_element, std::move(bubble_params));
  }

  if (help_bubble) {
    // TODO(crbug.com/40200981): Rewrite this when we have the ability for FE
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
    MaybeShowQueuedPromo();
  }
}

void FeaturePromoControllerCommon::OnHelpBubbleClosed(
    HelpBubble* bubble,
    HelpBubble::CloseReason reason) {
  // Since we're in the middle of processing callbacks we can't reset our
  // subscription but since it's a weak pointer (internally) and since we should
  // should only get called here once, it's not a big deal if we don't reset
  // it.
  bool closed_unexpectedly = false;
  if (bubble == promo_bubble()) {
    if (current_promo_->OnPromoBubbleClosed(reason)) {
      current_promo_.reset();
      closed_unexpectedly = true;
    }
  }

  if (bubble_closed_callback_) {
    std::move(bubble_closed_callback_).Run();
  }

  if (closed_unexpectedly) {
    MaybeShowQueuedPromo();
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
                     GetCommonWeakPtr(), base::Unretained(iph_feature)),
      base::BindOnce(&FeaturePromoControllerCommon::OnTutorialAborted,
                     GetCommonWeakPtr(), base::Unretained(iph_feature)));
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
FeaturePromoControllerCommon::CreateRotatingToastButtons(
    const base::Feature& feature) {
  // For now, use the same "got it" button as a snooze IPH that has run out of
  // snoozes.
  return CreateSnoozeButtons(feature, /*can_snooze=*/false);
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerCommon::CreateSnoozeButtons(const base::Feature& feature,
                                                  bool can_snooze) {
  std::vector<HelpBubbleButtonParams> buttons;

  if (can_snooze) {
    HelpBubbleButtonParams snooze_button;
    snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    snooze_button.is_default = false;
    snooze_button.callback =
        base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleSnoozed,
                       GetCommonWeakPtr(), base::Unretained(&feature));
    buttons.push_back(std::move(snooze_button));
  }

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
  dismiss_button.is_default = true;
  dismiss_button.callback =
      base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleDismissed,
                     GetCommonWeakPtr(), base::Unretained(&feature),
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
  action_button.callback = base::BindOnce(
      &FeaturePromoControllerCommon::OnCustomAction, GetCommonWeakPtr(),
      base::Unretained(&feature), custom_action_callback);
  buttons.push_back(std::move(action_button));

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text =
      l10n_util::GetStringUTF16(custom_action_dismiss_string_id);
  dismiss_button.is_default = !custom_action_is_default;
  dismiss_button.callback =
      base::BindOnce(&FeaturePromoControllerCommon::OnHelpBubbleDismissed,
                     GetCommonWeakPtr(), base::Unretained(&feature),
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
        GetCommonWeakPtr(), base::Unretained(&feature), tutorial_id);
  } else {
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
    dismiss_button.callback = base::BindRepeating(
        &FeaturePromoControllerCommon::OnTutorialHelpBubbleDismissed,
        GetCommonWeakPtr(), base::Unretained(&feature), tutorial_id);
  }
  buttons.push_back(std::move(dismiss_button));

  HelpBubbleButtonParams tutorial_button;
  tutorial_button.text =
      l10n_util::GetStringUTF16(IDS_PROMO_SHOW_TUTORIAL_BUTTON);
  tutorial_button.is_default = true;
  tutorial_button.callback = base::BindRepeating(
      &FeaturePromoControllerCommon::OnTutorialStarted, GetCommonWeakPtr(),
      base::Unretained(&feature), tutorial_id);
  buttons.push_back(std::move(tutorial_button));

  return buttons;
}

const base::Feature* FeaturePromoControllerCommon::GetCurrentPromoFeature()
    const {
  return current_promo_ ? current_promo_->iph_feature() : nullptr;
}

void FeaturePromoControllerCommon::RecordPromoNotShown(
    const char* feature_name,
    FeaturePromoResult::Failure failure) const {
  // Record Promo not shown.
  std::string action_name = "UserEducation.MessageNotShown";
  base::RecordComputedAction(action_name);

  // Record Failure as histogram.
  base::UmaHistogramEnumeration(action_name, failure);

  // Record Promo feature ID.
  action_name.append(".");
  action_name.append(feature_name);
  base::RecordComputedAction(action_name);

  // Record Failure as histogram with feature ID.
  base::UmaHistogramEnumeration(action_name, failure);

  // Record Failure as user action
  std::string failure_action_name = "UserEducation.MessageNotShown.";
  failure_action_name.append(FeaturePromoResult::GetFailureName(failure));
  base::RecordComputedAction(failure_action_name);
}

// static
bool FeaturePromoControllerCommon::active_window_check_blocked_ = false;

// static
FeaturePromoControllerCommon::TestLock
FeaturePromoControllerCommon::BlockActiveWindowCheckForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&active_window_check_blocked_,
                                                 true);
}

FeaturePromoParams::FeaturePromoParams(const base::Feature& iph_feature,
                                       const std::string& promo_key)
    : feature(iph_feature), key(promo_key) {}
FeaturePromoParams::FeaturePromoParams(FeaturePromoParams&& other) noexcept =
    default;
FeaturePromoParams& FeaturePromoParams::operator=(
    FeaturePromoParams&& other) noexcept = default;
FeaturePromoParams::~FeaturePromoParams() = default;

std::ostream& operator<<(std::ostream& os, FeaturePromoStatus status) {
  switch (status) {
    case FeaturePromoStatus::kBubbleShowing:
      os << "kBubbleShowing";
      break;
    case FeaturePromoStatus::kContinued:
      os << "kContinued";
      break;
    case FeaturePromoStatus::kNotRunning:
      os << "kNotRunning";
      break;
    case FeaturePromoStatus::kQueued:
      os << "kQueued";
      break;
  }
  return os;
}

}  // namespace user_education
