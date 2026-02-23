// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/common_preconditions.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_queue.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_queue_set.h"
#include "components/user_education/common/feature_promo/impl/messaging_coordinator.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

constexpr base::TimeDelta kDemoTimeout = base::Seconds(20);
constexpr FeaturePromoResult::Failure kDemoOverrideFailure =
    FeaturePromoResult::kBlockedByPromo;

std::string GetFeatureEngagementDemoFeatureName() {
  if (!base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode)) {
    return std::string();
  }

  return base::GetFieldTrialParamValueByFeature(
      feature_engagement::kIPHDemoMode,
      feature_engagement::kIPHDemoModeFeatureChoiceParam);
}

// Returns the most relevant valid context for a bubble that is already showing.
//
// Typically, this is the `bubble_context` - i.e. the context of the surface
// the bubble is actually showing in - but in some cases the context may be
// missing or invalid, in which case fall back to the `original_context` - the
// context of the surface the IPH was invoked from.
//
// If both contexts are missing or invalid, returns null. If a non-null value is
// returned, the context is guaranteed to be valid.
//
// It is not possible to always rely on the `original_context` because an IPH
// can be triggered from a browser window but appear on e.g. an App window, and
// while the promo is visible, the original browser window could be closed. The
// logic here provides the greatest chance at success in that situation, as well
// as ensuring that, if possible, follow-ups such as custom actions and
// tutorials will take place in the same window as the IPH.
UserEducationContextPtr ResolveContext(
    const UserEducationContextPtr& original_context,
    const UserEducationContextPtr& bubble_context) {
  if (bubble_context && bubble_context->IsValid()) {
    return bubble_context;
  }
  if (original_context->IsValid()) {
    return original_context;
  }
  return nullptr;
}

}  // namespace

struct FeaturePromoControllerImpl::PrivateData {
  PrivateData(const FeaturePromoPriorityProvider& priority_provider,
              const UserEducationTimeProvider& time_provider,
              ProductMessagingController& messaging_controller,
              feature_engagement::Tracker* tracker)
      : tracker_(tracker),
        demo_queue(demo_required, demo_wait_for, time_provider, kDemoTimeout),
        queues(priority_provider, time_provider),
        messaging_coordinator(messaging_controller) {
    queues.AddQueue(Priority::kHigh, high_priority_required,
                    high_priority_wait_for, features::GetHighPriorityTimeout());
    queues.AddQueue(Priority::kMedium, medium_priority_required,
                    medium_priority_wait_for,
                    features::GetMediumPriorityTimeout());
    queues.AddQueue(Priority::kLow, low_priority_required,
                    low_priority_wait_for, features::GetLowPriorityTimeout());
  }

  PrivateData(const PrivateData&) = delete;
  void operator=(const PrivateData&) = delete;
  ~PrivateData() = default;

  const FeatureEngagementTrackerInitializedPrecondition&
  get_tracker_precondition() {
    if (!tracker_initialized_precondition_) {
      tracker_initialized_precondition_ =
          std::make_unique<FeatureEngagementTrackerInitializedPrecondition>(
              tracker_.get());
    }
    return *tracker_initialized_precondition_;
  }

  const raw_ptr<feature_engagement::Tracker> tracker_;
  std::unique_ptr<FeatureEngagementTrackerInitializedPrecondition>
      tracker_initialized_precondition_;
  ComposingPreconditionListProvider demo_required;
  ComposingPreconditionListProvider high_priority_required;
  ComposingPreconditionListProvider medium_priority_required;
  ComposingPreconditionListProvider low_priority_required;
  ComposingPreconditionListProvider demo_wait_for;
  ComposingPreconditionListProvider high_priority_wait_for;
  ComposingPreconditionListProvider medium_priority_wait_for;
  ComposingPreconditionListProvider low_priority_wait_for;
  internal::FeaturePromoQueue demo_queue;
  internal::FeaturePromoQueueSet queues;
  internal::MessagingCoordinator messaging_coordinator;
  base::CallbackListSubscription ready_subscription;
  base::CallbackListSubscription preempted_subscription;
};

struct FeaturePromoControllerImpl::PromoData {
  PromoData() = default;
  PromoData(PromoData&&) noexcept = default;
  PromoData& operator=(PromoData&&) noexcept = default;
  ~PromoData() = default;

  // The promo that should show right now.
  std::optional<internal::EligibleFeaturePromo> eligible_promo;
  // Whether `show_params` is for the demo page.
  bool for_demo = false;
  // The priority of the promo that would show if the priority handle is held.
  std::optional<Priority> pending_priority;

  FeaturePromoParams& params() { return eligible_promo->promo_params; }
  UserEducationContextPtr& context() { return eligible_promo->promo_context; }

  ui::TrackedElement* GetAnchorElement() {
    return eligible_promo
        ->cached_data[AnchorElementPrecondition::kAnchorElement]
        .get();
  }

  std::unique_ptr<FeaturePromoLifecycle>& GetLifecycle() {
    return eligible_promo->cached_data[LifecyclePrecondition::kLifecycle];
  }

  const base::Feature& GetFeature() const {
    return *eligible_promo->promo_params.feature;
  }
};

FeaturePromoControllerImpl::FeaturePromoControllerImpl(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    UserEducationStorageService* storage_service,
    FeaturePromoSessionPolicy* session_policy,
    TutorialService* tutorial_service,
    ProductMessagingController* messaging_controller)
    : registry_(registry),
      feature_engagement_tracker_(feature_engagement_tracker),
      bubble_factory_registry_(help_bubble_registry),
      storage_service_(storage_service),
      session_policy_(session_policy),
      tutorial_service_(tutorial_service),
      product_messaging_controller_(*messaging_controller),
      demo_feature_name_(GetFeatureEngagementDemoFeatureName()) {
  DCHECK(feature_engagement_tracker_);
  DCHECK(bubble_factory_registry_);
  DCHECK(storage_service_);
}

void FeaturePromoControllerImpl::Init() {
  private_ = std::make_unique<PrivateData>(
      *session_policy(), *storage_service(), *product_messaging_controller_,
      feature_engagement_tracker());
  AddDemoPreconditionProviders(private_->demo_required, true);
  AddDemoPreconditionProviders(private_->demo_wait_for, false);
  AddPreconditionProviders(private_->high_priority_required, Priority::kHigh,
                           true);
  AddPreconditionProviders(private_->high_priority_wait_for, Priority::kHigh,
                           false);
  AddPreconditionProviders(private_->medium_priority_required,
                           Priority::kMedium, true);
  AddPreconditionProviders(private_->medium_priority_wait_for,
                           Priority::kMedium, false);
  AddPreconditionProviders(private_->low_priority_required, Priority::kLow,
                           true);
  AddPreconditionProviders(private_->low_priority_wait_for, Priority::kLow,
                           false);
  private_->ready_subscription =
      private_->messaging_coordinator.AddPromoReadyCallback(
          base::BindRepeating(&FeaturePromoControllerImpl::OnMessagingPriority,
                              weak_ptr_factory_.GetWeakPtr()));
  private_->preempted_subscription =
      private_->messaging_coordinator.AddPromoPreemptedCallback(
          base::BindRepeating(&FeaturePromoControllerImpl::OnPromoPreempted,
                              weak_ptr_factory_.GetWeakPtr()));
}

FeaturePromoControllerImpl::~FeaturePromoControllerImpl() {
  // Ensure that `OnDestroying()` was properly called.
  CHECK(!private_);
}

FeaturePromoResult FeaturePromoControllerImpl::CanShowPromo(
    const FeaturePromoParams& params,
    const UserEducationContextPtr& context) const {
  auto* const spec = registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    return FeaturePromoResult::kError;
  }
  auto result = private_->queues.CanShow(*spec, params, context);
  if (result && !private_->tracker_->WouldTriggerHelpUI(*params.feature)) {
    // This can happen if a non-IPH promo is showing.
    // These are strongly advised against but they can happen.
    result = FeaturePromoResult::kBlockedByConfig;
  }
  return result;
}

void FeaturePromoControllerImpl::MaybeShowStartupPromo(
    FeaturePromoParams params,
    UserEducationContextPtr context) {
  // Don't repeatedly attempt to show startup promos. Once one has been queued,
  // subsequent attempts will fail.
  if (attempted_startup_promos_.contains(params.feature)) {
    PostShowPromoResult(*params.feature,
                        std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kAlreadyQueued);
    return;
  }
  attempted_startup_promos_.emplace(params.feature);
  MaybeShowPromo(std::move(params), std::move(context));
}

void FeaturePromoControllerImpl::MaybeShowPromo(
    FeaturePromoParams params,
    UserEducationContextPtr context) {
  auto* const spec = registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    PostShowPromoResult(*params.feature,
                        std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kError);
    return;
  }

  if (current_promo() &&
      current_promo()->iph_feature() == &params.feature.get()) {
    PostShowPromoResult(*params.feature,
                        std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kAlreadyQueued);
    return;
  }

  // If the context is not valid, it should abort immediately.
  if (!context->IsValid()) {
    PostShowPromoResult(*params.feature,
                        std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kAnchorNotVisible);
    return;
  }

  params.show_promo_result_callback = base::BindOnce(
      [](base::WeakPtr<FeaturePromoControllerImpl> controller,
         std::string feature_name,
         FeaturePromoController::ShowPromoResultCallback callback,
         FeaturePromoResult result) {
        if (controller && !result) {
          if (result == FeaturePromoResult::kTimedOut &&
              (controller->current_promo() ||
               controller->bubble_factory_registry()->is_any_bubble_showing() ||
               controller->private_->messaging_coordinator
                   .IsBlockedByExternalPromo())) {
            result = FeaturePromoResult::kBlockedByPromo;
          }
          controller->RecordPromoNotShown(feature_name.c_str(),
                                          *result.failure());
        }
        if (callback) {
          std::move(callback).Run(result);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), params.feature->name,
      std::move(params.show_promo_result_callback));

  private_->queues.TryToQueue(*spec, std::move(params), std::move(context));
  MaybePostUpdate();
}

void FeaturePromoControllerImpl::MaybeShowPromoForDemoPage(
    FeaturePromoParams params,
    UserEducationContextPtr context) {
  auto* const spec = registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    PostShowPromoResult(*params.feature,
                        std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kError);
    return;
  }

  if (auto* const feature = GetCurrentPromoFeature()) {
    // Note that overriding for promo or precedence doesn't trigger an update.
    EndPromo(*feature, FeaturePromoClosedReason::kOverrideForDemo);
  }
  private_->demo_queue.FailAll(FeaturePromoResult::kCanceled);
  private_->demo_queue.TryToQueue(*spec, std::move(params), std::move(context));

  // This can happen immediately since the message to show the promo will come
  // in on a (mostly) fresh call stack.
  UpdateQueuesAndMaybeShowPromo();
}

bool FeaturePromoControllerImpl::IsPromoQueued(
    const base::Feature& iph_feature) const {
  return private_->demo_queue.IsQueued(iph_feature) ||
         private_->queues.IsQueued(iph_feature);
}

bool FeaturePromoControllerImpl::MaybeUnqueuePromo(
    const base::Feature& iph_feature) {
  const bool canceled_demo = private_->demo_queue.Cancel(iph_feature);
  const bool canceled_queued = private_->queues.Cancel(iph_feature);
  if (!canceled_demo && !canceled_queued) {
    return false;
  }

  // Need to update the queue state if something was removed.
  MaybePostUpdate();
  return true;
}

void FeaturePromoControllerImpl::MaybeShowQueuedPromo() {
  // These are typically called when another promo ends, so update on a fresh
  // call stack.
  MaybePostUpdate();
}

base::WeakPtr<FeaturePromoController>
FeaturePromoControllerImpl::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<FeaturePromoControllerImpl>
FeaturePromoControllerImpl::GetImplWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FeaturePromoControllerImpl::IsDemoPending() const {
  return !private_->demo_queue.is_empty();
}

bool FeaturePromoControllerImpl::IsPromoQueued() const {
  return !private_->queues.IsEmpty();
}

FeaturePromoStatus FeaturePromoControllerImpl::GetPromoStatus(
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
FeaturePromoControllerImpl::GetCurrentPromoSpecificationForAnchor(
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

bool FeaturePromoControllerImpl::HasPromoBeenDismissed(
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
      return data->shown_for_keys.contains(params.key);
  }
}

bool FeaturePromoControllerImpl::EndPromo(
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

bool FeaturePromoControllerImpl::EndPromo(
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

void FeaturePromoControllerImpl::CloseHelpBubbleIfPresent(
    ui::ElementContext context) {
  if (auto* const help_bubble =
          bubble_factory_registry()->GetHelpBubble(context)) {
    help_bubble->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
  }
}

void FeaturePromoControllerImpl::RecordPromoEnded(
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

bool FeaturePromoControllerImpl::DismissNonCriticalBubbleInRegion(
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
void FeaturePromoControllerImpl::NotifyFeatureUsedIfValid(
    const base::Feature& feature) {
  if (base::FeatureList::IsEnabled(feature) &&
      registry_->IsFeatureRegistered(feature)) {
    feature_engagement_tracker_->NotifyUsedEvent(feature);
  }
}
#endif

FeaturePromoHandle FeaturePromoControllerImpl::CloseBubbleAndContinuePromo(
    const base::Feature& iph_feature) {
  return CloseBubbleAndContinuePromoWithReason(
      iph_feature, FeaturePromoClosedReason::kFeatureEngaged);
}

FeaturePromoHandle
FeaturePromoControllerImpl::CloseBubbleAndContinuePromoWithReason(
    const base::Feature& iph_feature,
    FeaturePromoClosedReason close_reason) {
  DCHECK_EQ(GetCurrentPromoFeature(), &iph_feature);
  RecordPromoEnded(close_reason, /*continue_after_close=*/true);
  return FeaturePromoHandle(GetAsWeakPtr(), &iph_feature);
}

FeaturePromoControllerImpl::PromoData
FeaturePromoControllerImpl::GetNextPromoData() {
  PromoData result;

  if (current_promo() && current_promo()->is_demo()) {
    // If a demo promo is showing, all other promos fail immediately.
    // New demo promos will also clear out the current promo and fail all promos
    // in the queue.
    private_->queues.FailAll(kDemoOverrideFailure);
  } else if (IsDemoPending()) {
    // Demo takes precedence over other promos.
    result.for_demo = true;
    if (const auto* const pending =
            private_->demo_queue.UpdateAndIdentifyNextEligiblePromo()) {
      result.eligible_promo =
          private_->demo_queue.UnqueueEligiblePromo(*pending);
      private_->queues.FailAll(kDemoOverrideFailure);
    }
  } else if (const auto pending =
                 private_->queues.UpdateAndIdentifyNextEligiblePromo()) {
    // If there's a pending promo determine if it can show right now.
    const bool is_high_priority =
        pending->second == FeaturePromoPriorityProvider::PromoPriority::kHigh;
    if (private_->messaging_coordinator.CanShowPromo(is_high_priority)) {
      result.eligible_promo = private_->queues.UnqueueEligiblePromo(*pending);
    } else {
      result.pending_priority = pending->second;
    }
  }

  return result;
}

FeaturePromoResult FeaturePromoControllerImpl::ShowPromo(
    PromoData& promo_data) {
  auto* const spec = registry()->GetParamsForFeature(promo_data.GetFeature());
  CHECK(spec);
  auto* const anchor_element = promo_data.GetAnchorElement();
  CHECK(anchor_element);
  auto& lifecycle = promo_data.GetLifecycle();
  const base::Feature& feature = promo_data.GetFeature();
  const bool in_demo_mode =
      (GetFeatureEngagementDemoFeatureName() == feature.name);
  auto* const index = promo_data.eligible_promo->cached_data.GetIfPresent(
      AnchorElementPrecondition::kRotatingPromoIndex);
  auto* display_spec = spec;
  if (index && index->has_value()) {
    lifecycle->SetPromoIndex(index->value());
    display_spec = &*spec->rotating_promos().at(index->value());
  }

  // Construct the parameters for the promotion.
  FeaturePromoSpecification::BuildHelpBubbleParams build_params;
  build_params.spec = display_spec;
  build_params.anchor_element = anchor_element;
  build_params.body_format = std::move(promo_data.params().body_params);
  build_params.screen_reader_format =
      std::move(promo_data.params().screen_reader_params);
  build_params.title_format = std::move(promo_data.params().title_params);
  build_params.can_snooze = promo_data.GetLifecycle()->CanSnooze();
  build_params.arrow =
      build_params.spec->GetBubbleArrow(build_params.anchor_element);

  // If the session policy allows overriding the current promo, abort it.
  if (current_promo()) {
    // Note that this does not trigger an update, since overrides by default do
    // not trigger updates.
    EndPromo(*GetCurrentPromoFeature(),
             promo_data.for_demo
                 ? FeaturePromoClosedReason::kOverrideForDemo
                 : FeaturePromoClosedReason::kOverrideForPrecedence);
  }

  // TODO(crbug.com/40200981): Currently this must be called before
  // ShouldTriggerHelpUI() below. See bug for details.
  if (build_params.spec->promo_type() !=
      FeaturePromoSpecification::PromoType::kCustomUi) {
    build_params.screen_reader_prompt_available =
        CheckExtendedPropertiesPromptAvailable(promo_data.for_demo ||
                                               in_demo_mode);
  }

  // When not explicitly for a demo, notify the tracker that the promo is
  // starting. Since this is also one of the preconditions for the promo,
  //
  if (!promo_data.for_demo &&
      !feature_engagement_tracker()->ShouldTriggerHelpUI(feature)) {
    return FeaturePromoResult::kBlockedByConfig;
  }

  // If the session policy allows overriding other help bubbles, close them.
  CloseHelpBubbleIfPresent(anchor_element->context());

  // Store the current promo.
  set_current_promo(std::move(lifecycle));

  // Try to show the bubble and bail out if we cannot.
  auto bubble = ShowPromoBubbleImpl(std::move(build_params),
                                    std::move(promo_data.context()));
  if (!bubble) {
    set_current_promo(nullptr);
    if (!promo_data.for_demo) {
      feature_engagement_tracker()->Dismissed(feature);
    }
    return FeaturePromoResult::kError;
  }

  // Update the most recent promo info.
  set_last_promo_info(session_policy()->GetPromoPriorityInfo(*spec));
  session_policy()->NotifyPromoShown(last_promo_info());

  set_bubble_closed_callback(std::move(promo_data.params().close_callback));

  if (promo_data.for_demo) {
    current_promo()->OnPromoShownForDemo(std::move(bubble));
  } else {
    current_promo()->OnPromoShown(std::move(bubble),
                                  feature_engagement_tracker());
  }

  return FeaturePromoResult::Success();
}

void FeaturePromoControllerImpl::MaybePostUpdate() {
  if (queue_update_pending_ || poller_.IsRunning()) {
    return;
  }
  queue_update_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FeaturePromoControllerImpl::UpdateQueuesAndMaybeShowPromo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FeaturePromoControllerImpl::UpdateQueuesAndMaybeShowPromo() {
  using PromoState = internal::MessagingCoordinator::PromoState;
  queue_update_pending_ = false;

  // Fetch the next promo to be shown, or promo waiting to be shown.
  PromoData promo_data = GetNextPromoData();
  if (promo_data.eligible_promo) {
    const auto result = ShowPromo(promo_data);

    // Need to send the result regardless of success or failure.
    PostShowPromoResult(
        *promo_data.params().feature,
        std::move(promo_data.params().show_promo_result_callback), result);

    // On failure to show, try to poll the queue again immediately.
    if (!result) {
      UpdateQueuesAndMaybeShowPromo();
      return;
    }

    // Update the showing state based on the type of promo shown; demo page
    // demos do not count as promos for messaging coordination purposes.
    const PromoState new_state =
        current_promo()->is_demo()
            ? PromoState::kNone
            : (last_promo_info().priority == Priority::kHigh
                   ? PromoState::kHighPriorityShowing
                   : PromoState::kLowPriorityShowing);
    private_->messaging_coordinator.TransitionToState(new_state);
    UpdatePollingState();
    return;
  }

  // Update the promo state based on whether there is a non-demo promo pending.
  // It's okay to transition to a pending state, because if either high or low
  // priority were allowed and there were eligible promos, one would have been
  // shown above.
  std::optional<PromoState> new_state;
  if (IsDemoPending() || private_->queues.IsEmpty() ||
      !promo_data.pending_priority) {
    if (!current_promo()) {
      new_state = PromoState::kNone;
    }
  } else if (*promo_data.pending_priority == Priority::kHigh) {
    new_state = PromoState::kHighPriorityPending;
  } else {
    new_state = PromoState::kLowPriorityPending;
  }
  if (new_state) {
    private_->messaging_coordinator.TransitionToState(*new_state);
  }
  UpdatePollingState();
}

void FeaturePromoControllerImpl::UpdatePollingState() {
  const bool should_poll =
      !private_->demo_queue.is_empty() || !private_->queues.IsEmpty();
  if (poller_.IsRunning()) {
    if (!should_poll) {
      poller_.Stop();
    }
  } else if (should_poll) {
    poller_.Start(
        FROM_HERE, features::GetPromoControllerPollingInterval(),
        base::BindRepeating(
            &FeaturePromoControllerImpl::UpdateQueuesAndMaybeShowPromo,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void FeaturePromoControllerImpl::OnMessagingPriority() {
  // This is always called on a fresh stack so no need to post the update.
  UpdateQueuesAndMaybeShowPromo();
}

void FeaturePromoControllerImpl::OnPromoPreempted() {
  // This should only be called when a low-priority promo is showing.
  const auto* const current_promo = GetCurrentPromoFeature();
  CHECK(current_promo);
  CHECK(last_promo_info().priority != Priority::kHigh);
  EndPromo(*current_promo, FeaturePromoClosedReason::kOverrideForPrecedence);
  // Ending due to override does not automatically trigger an update. Even
  // though promos are currently preempted, the state of the queues,
  // coordinator, and polling must be updated.
  MaybePostUpdate();
}

void FeaturePromoControllerImpl::OnDestroying() {
  poller_.Stop();
  private_.reset();
}

// Default precondition providers are all created down here.

void FeaturePromoControllerImpl::AddDemoPreconditionProviders(
    ComposingPreconditionListProvider& to_add_to,
    bool required) {
  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoControllerImpl> controller,
           const FeaturePromoSpecification& spec,
           const FeaturePromoParams& params,
           const UserEducationContextPtr& context) {
          FeaturePromoPreconditionList list;
          // Ensure the surface is not gone.
          list.AddPrecondition(
              std::make_unique<ContextValidPrecondition>(context));
          if (auto* const ptr = controller.get()) {
            list.AddPrecondition(std::make_unique<LifecyclePrecondition>(
                ptr->CreateLifecycleFor(spec, params), /*for_demo=*/true));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoControllerImpl> controller,
           const FeaturePromoSpecification& spec, const FeaturePromoParams&,
           const UserEducationContextPtr& context) {
          FeaturePromoPreconditionList list;
          if (auto* const ptr = controller.get()) {
            // In demos, when repeating the same repeating promo to test it, the
            // index should cycle. However, the updated index is not written
            // until the previous promo is ended, which happens later. In order
            // to simulate this, base the starting index off the one being used
            // by the previous promo.
            const bool pre_increment =
                ptr->current_promo() &&
                ptr->current_promo()->iph_feature() == spec.feature();
            list.AddPrecondition(std::make_unique<AnchorElementPrecondition>(
                spec, context->GetElementContext(), pre_increment));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  }
}

void FeaturePromoControllerImpl::AddPreconditionProviders(
    ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoControllerImpl> controller,
           const FeaturePromoSpecification& spec,
           const FeaturePromoParams& params,
           const UserEducationContextPtr& context) {
          FeaturePromoPreconditionList list;
          if (auto* const ptr = controller.get()) {
            // First ensure that the feature is enabled.
            list.AddPrecondition(
                std::make_unique<FeatureEnabledPrecondition>(*params.feature));
            // Next verify that the feature has not been dismissed and is not
            // blocked by other profile-based considerations.
            const bool for_demo =
                ptr->demo_feature_name_ == spec.feature()->name;
            list.AddPrecondition(std::make_unique<LifecyclePrecondition>(
                ptr->CreateLifecycleFor(spec, params), for_demo));
            // Next, verify that the promo is not excluded by any events or
            // additional conditions.
            list.AddPrecondition(
                std::make_unique<MeetsFeatureEngagementCriteriaPrecondition>(
                    *params.feature, *ptr->feature_engagement_tracker()));
            // Next, verify that the promo is eligible to show based on
            // session policy.
            list.AddPrecondition(std::make_unique<SessionPolicyPrecondition>(
                ptr->session_policy(),
                ptr->session_policy()->GetPromoPriorityInfo(spec),
                base::BindRepeating([]() {
                  return std::optional<
                      FeaturePromoPriorityProvider::PromoPriorityInfo>();
                })));
            // Finally, ensure the target surface is still present.
            list.AddPrecondition(
                std::make_unique<ContextValidPrecondition>(context));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoControllerImpl> controller,
           const FeaturePromoSpecification& spec, const FeaturePromoParams&,
           const UserEducationContextPtr& context) {
          FeaturePromoPreconditionList list;
          if (auto* const ptr = controller.get()) {
            list.AddPrecondition(
                std::make_unique<ForwardingFeaturePromoPrecondition>(
                    ptr->private_->get_tracker_precondition()));
            list.AddPrecondition(std::make_unique<AnchorElementPrecondition>(
                spec, context->GetElementContext(), false));
            // Wait-for state *does* take the current promo into account, since
            // a higher-weight promo might block a lower-weight promo.
            list.AddPrecondition(std::make_unique<SessionPolicyPrecondition>(
                ptr->session_policy(),
                ptr->session_policy()->GetPromoPriorityInfo(spec),
                base::BindRepeating(
                    [](base::WeakPtr<FeaturePromoControllerImpl> controller) {
                      if (auto* const ptr = controller.get()) {
                        return ptr->current_promo()
                                   ? std::make_optional(ptr->last_promo_info())
                                   : std::nullopt;
                      } else {
                        return std::optional<
                            FeaturePromoPriorityProvider::PromoPriorityInfo>();
                      }
                    },
                    controller)));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  }
}

bool FeaturePromoControllerImpl::CheckExtendedPropertiesPromptAvailable(
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
FeaturePromoControllerImpl::CreateLifecycleFor(
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

std::unique_ptr<HelpBubble> FeaturePromoControllerImpl::ShowPromoBubbleImpl(
    FeaturePromoSpecification::BuildHelpBubbleParams params,
    UserEducationContextPtr context) {
  CHECK(context && context->IsValid());
  const auto& spec = *params.spec;
  const auto* const accelerator_provider = context->GetAcceleratorProvider();
  const auto bubble_context = GetContextForHelpBubble(params.anchor_element);

  bool had_screen_reader_promo = false;
  std::unique_ptr<HelpBubble> help_bubble;
  if (spec.promo_type() == FeaturePromoSpecification::PromoType::kCustomUi) {
    auto result = spec.BuildCustomHelpBubble(context, std::move(params));
    help_bubble = std::move(std::get<std::unique_ptr<HelpBubble>>(result));
    CHECK(help_bubble);
    auto* const ui = std::get<base::WeakPtr<CustomHelpBubbleUi>>(result).get();
    CHECK(ui);
    custom_ui_result_subscription_ =
        ui->AddUserActionCallback(base::BindRepeating(
            [](FeaturePromoControllerImpl* controller,
               const FeaturePromoSpecification* spec,
               const UserEducationContextPtr& context,
               const UserEducationContextPtr& bubble_context,
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
                  controller->OnCustomAction(spec->feature(), context,
                                             bubble_context,
                                             spec->custom_action_callback());
                  break;
                case CustomHelpBubbleUi::UserAction::kSnooze:
                  controller->OnHelpBubbleSnoozed(spec->feature());
                  break;
              }
            },
            base::Unretained(this), base::Unretained(&spec), context,
            bubble_context));
  } else {
    HelpBubbleParams bubble_params;
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
      bubble_params.screenreader_text =
          l10n_util::GetStringFUTF16(spec.screen_reader_string_id(),
                                     spec.screen_reader_accelerator()
                                         .GetAccelerator(accelerator_provider)
                                         .GetShortcutText());
    } else {
      bubble_params.screenreader_text = FeaturePromoSpecification::FormatString(
          spec.screen_reader_string_id(),
          std::move(params.screen_reader_format));
    }
    bubble_params.close_button_alt_text =
        l10n_util::GetStringUTF16(IDS_CLOSE_PROMO);
    bubble_params.body_icon = spec.bubble_icon();
    if (bubble_params.body_icon) {
      bubble_params.body_icon_alt_text = GetBodyIconAltText();
    }
    bubble_params.arrow = params.arrow;
    bubble_params.focus_on_show_hint = spec.focus_on_show_override();

    bubble_params.timeout_callback =
        base::BindOnce(&FeaturePromoControllerImpl::OnHelpBubbleTimeout,
                       GetImplWeakPtr(), base::Unretained(spec.feature()));

    // Feature isn't present for some critical promos.
    if (spec.feature()) {
      bubble_params.dismiss_callback =
          base::BindOnce(&FeaturePromoControllerImpl::OnHelpBubbleDismissed,
                         GetImplWeakPtr(), base::Unretained(spec.feature()),
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
        bubble_params.buttons =
            CreateTutorialButtons(*spec.feature(), context, bubble_context,
                                  params.can_snooze, spec.tutorial_id());
        bubble_params.dismiss_callback = base::BindOnce(
            &FeaturePromoControllerImpl::OnTutorialHelpBubbleDismissed,
            GetImplWeakPtr(), base::Unretained(spec.feature()),
            spec.tutorial_id());
        break;
      case FeaturePromoSpecification::PromoType::kCustomAction:
        CHECK(spec.feature());
        bubble_params.buttons = CreateCustomActionButtons(
            *spec.feature(), context, bubble_context,
            spec.custom_action_caption(), spec.custom_action_callback(),
            spec.custom_action_is_default(),
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
      bubble_params.keyboard_navigation_hint =
          GetTutorialScreenReaderHint(accelerator_provider);
    } else if (params.screen_reader_prompt_available) {
      bubble_params.keyboard_navigation_hint =
          GetFocusHelpBubbleScreenReaderHint(
              spec.promo_type(), params.anchor_element, accelerator_provider);
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
        base::BindOnce(&FeaturePromoControllerImpl::OnHelpBubbleClosed,
                       base::Unretained(this)));
  }

  return help_bubble;
}

void FeaturePromoControllerImpl::FinishContinuedPromo(
    const base::Feature& iph_feature) {
  if (GetCurrentPromoFeature() == &iph_feature) {
    current_promo_->OnContinuedPromoEnded(/*completed_successfully=*/true);
    current_promo_.reset();
    MaybeShowQueuedPromo();
  }
}

void FeaturePromoControllerImpl::OnHelpBubbleClosed(
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

void FeaturePromoControllerImpl::OnHelpBubbleTimedOut(
    const base::Feature* feature) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(FeaturePromoClosedReason::kTimeout,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerImpl::OnHelpBubbleSnoozed(
    const base::Feature* feature) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(FeaturePromoClosedReason::kSnooze,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerImpl::OnHelpBubbleDismissed(
    const base::Feature* feature,
    bool via_action_button) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(via_action_button ? FeaturePromoClosedReason::kDismiss
                                       : FeaturePromoClosedReason::kCancel,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerImpl::OnHelpBubbleTimeout(
    const base::Feature* feature) {
  if (feature == GetCurrentPromoFeature()) {
    RecordPromoEnded(FeaturePromoClosedReason::kTimeout,
                     /*continue_after_close=*/false);
  }
}

void FeaturePromoControllerImpl::OnCustomAction(
    const base::Feature* feature,
    const UserEducationContextPtr& context,
    const UserEducationContextPtr& bubble_context,
    FeaturePromoSpecification::CustomActionCallback callback) {
  if (auto actual_context = ResolveContext(context, bubble_context)) {
    callback.Run(actual_context,
                 CloseBubbleAndContinuePromoWithReason(
                     *feature, FeaturePromoClosedReason::kAction));
  }
}

void FeaturePromoControllerImpl::OnTutorialHelpBubbleSnoozed(
    const base::Feature* iph_feature,
    TutorialIdentifier tutorial_id) {
  OnHelpBubbleSnoozed(iph_feature);
  tutorial_service_->LogIPHLinkClicked(tutorial_id, false);
}

void FeaturePromoControllerImpl::OnTutorialHelpBubbleDismissed(
    const base::Feature* iph_feature,
    TutorialIdentifier tutorial_id) {
  OnHelpBubbleDismissed(iph_feature,
                        /* via_action_button =*/true);
  tutorial_service_->LogIPHLinkClicked(tutorial_id, false);
}

void FeaturePromoControllerImpl::OnTutorialStarted(
    const base::Feature* iph_feature,
    const UserEducationContextPtr& context,
    const UserEducationContextPtr& bubble_context,
    TutorialIdentifier tutorial_id) {
  DCHECK_EQ(GetCurrentPromoFeature(), iph_feature);
  tutorial_promo_handle_ = CloseBubbleAndContinuePromoWithReason(
      *iph_feature, FeaturePromoClosedReason::kAction);
  DCHECK(tutorial_promo_handle_.is_valid());
  if (auto actual_context = ResolveContext(context, bubble_context)) {
    tutorial_service_->StartTutorial(
        tutorial_id, context->GetElementContext(),
        base::BindOnce(&FeaturePromoControllerImpl::OnTutorialComplete,
                       GetImplWeakPtr(), base::Unretained(iph_feature)),
        base::BindOnce(&FeaturePromoControllerImpl::OnTutorialAborted,
                       GetImplWeakPtr(), base::Unretained(iph_feature)));
    if (tutorial_service_->IsRunningTutorial()) {
      tutorial_service_->LogIPHLinkClicked(tutorial_id, true);
    }
  }
}

void FeaturePromoControllerImpl::OnTutorialComplete(
    const base::Feature* iph_feature) {
  tutorial_promo_handle_.Release();
  if (GetCurrentPromoFeature() == iph_feature) {
    current_promo_->OnContinuedPromoEnded(/*completed_successfully=*/true);
    current_promo_.reset();
  }
}

void FeaturePromoControllerImpl::OnTutorialAborted(
    const base::Feature* iph_feature) {
  tutorial_promo_handle_.Release();
  if (GetCurrentPromoFeature() == iph_feature) {
    current_promo_->OnContinuedPromoEnded(/*completed_successfully=*/false);
    current_promo_.reset();
  }
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerImpl::CreateRotatingToastButtons(
    const base::Feature& feature) {
  // For now, use the same "got it" button as a snooze IPH that has run out of
  // snoozes.
  return CreateSnoozeButtons(feature, /*can_snooze=*/false);
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerImpl::CreateSnoozeButtons(const base::Feature& feature,
                                                bool can_snooze) {
  std::vector<HelpBubbleButtonParams> buttons;

  if (can_snooze) {
    HelpBubbleButtonParams snooze_button;
    snooze_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    snooze_button.is_default = false;
    snooze_button.callback =
        base::BindOnce(&FeaturePromoControllerImpl::OnHelpBubbleSnoozed,
                       GetImplWeakPtr(), base::Unretained(&feature));
    buttons.push_back(std::move(snooze_button));
  }

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
  dismiss_button.is_default = true;
  dismiss_button.callback =
      base::BindOnce(&FeaturePromoControllerImpl::OnHelpBubbleDismissed,
                     GetImplWeakPtr(), base::Unretained(&feature),
                     /* via_action_button =*/true);
  buttons.push_back(std::move(dismiss_button));

  return buttons;
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerImpl::CreateCustomActionButtons(
    const base::Feature& feature,
    const UserEducationContextPtr& context,
    const UserEducationContextPtr& bubble_context,
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
      base::BindOnce(&FeaturePromoControllerImpl::OnCustomAction,
                     GetImplWeakPtr(), base::Unretained(&feature), context,
                     bubble_context, custom_action_callback);
  buttons.push_back(std::move(action_button));

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.text =
      l10n_util::GetStringUTF16(custom_action_dismiss_string_id);
  dismiss_button.is_default = !custom_action_is_default;
  dismiss_button.callback =
      base::BindOnce(&FeaturePromoControllerImpl::OnHelpBubbleDismissed,
                     GetImplWeakPtr(), base::Unretained(&feature),
                     /* via_action_button =*/true);
  buttons.push_back(std::move(dismiss_button));

  return buttons;
}

std::vector<HelpBubbleButtonParams>
FeaturePromoControllerImpl::CreateTutorialButtons(
    const base::Feature& feature,
    const UserEducationContextPtr& context,
    const UserEducationContextPtr& bubble_context,
    bool can_snooze,
    TutorialIdentifier tutorial_id) {
  std::vector<HelpBubbleButtonParams> buttons;

  HelpBubbleButtonParams dismiss_button;
  dismiss_button.is_default = false;
  if (can_snooze) {
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    dismiss_button.callback = base::BindRepeating(
        &FeaturePromoControllerImpl::OnTutorialHelpBubbleSnoozed,
        GetImplWeakPtr(), base::Unretained(&feature), tutorial_id);
  } else {
    dismiss_button.text = l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
    dismiss_button.callback = base::BindRepeating(
        &FeaturePromoControllerImpl::OnTutorialHelpBubbleDismissed,
        GetImplWeakPtr(), base::Unretained(&feature), tutorial_id);
  }
  buttons.push_back(std::move(dismiss_button));

  HelpBubbleButtonParams tutorial_button;
  tutorial_button.text =
      l10n_util::GetStringUTF16(IDS_PROMO_SHOW_TUTORIAL_BUTTON);
  tutorial_button.is_default = true;
  tutorial_button.callback = base::BindRepeating(
      &FeaturePromoControllerImpl::OnTutorialStarted, GetImplWeakPtr(),
      base::Unretained(&feature), context, bubble_context, tutorial_id);
  buttons.push_back(std::move(tutorial_button));

  return buttons;
}

const base::Feature* FeaturePromoControllerImpl::GetCurrentPromoFeature()
    const {
  return current_promo_ ? current_promo_->iph_feature() : nullptr;
}

void FeaturePromoControllerImpl::RecordPromoNotShown(
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
bool FeaturePromoControllerImpl::active_window_check_blocked_ = false;

// static
FeaturePromoControllerImpl::TestLock
FeaturePromoControllerImpl::BlockActiveWindowCheckForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&active_window_check_blocked_,
                                                 true);
}

}  // namespace user_education
