// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_controller_25.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/interaction/element_tracker.h"

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

}  // namespace

struct FeaturePromoController25::PrivateData {
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

struct FeaturePromoController25::PromoData {
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

  ui::TrackedElement* GetAnchorElement() {
    const auto* const ref = internal::PreconditionData::Get(
        eligible_promo->cached_data, AnchorElementPrecondition::kAnchorElement);
    CHECK(ref) << "Expected anchor element precondition to have run.";
    return ref->get();
  }

  std::unique_ptr<FeaturePromoLifecycle>& GetLifecycle() {
    return *internal::PreconditionData::Get(eligible_promo->cached_data,
                                            LifecyclePrecondition::kLifecycle);
  }

  const base::Feature& GetFeature() const {
    return *eligible_promo->promo_params.feature;
  }
};

FeaturePromoController25::FeaturePromoController25(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    UserEducationStorageService* storage_service,
    FeaturePromoSessionPolicy* session_policy,
    TutorialService* tutorial_service,
    ProductMessagingController* messaging_controller)
    : FeaturePromoControllerCommon(feature_engagement_tracker,
                                   registry,
                                   help_bubble_registry,
                                   storage_service,
                                   session_policy,
                                   tutorial_service),
      demo_feature_name_(GetFeatureEngagementDemoFeatureName()),
      product_messaging_controller_(*messaging_controller) {}

void FeaturePromoController25::Init() {
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
          base::BindRepeating(&FeaturePromoController25::OnMessagingPriority,
                              weak_ptr_factory_.GetWeakPtr()));
  private_->preempted_subscription =
      private_->messaging_coordinator.AddPromoPreemptedCallback(
          base::BindRepeating(&FeaturePromoController25::OnPromoPreempted,
                              weak_ptr_factory_.GetWeakPtr()));
}

FeaturePromoController25::~FeaturePromoController25() {
  // Ensure that `OnDestroying()` was properly called.
  CHECK(!private_);
}

FeaturePromoResult FeaturePromoController25::CanShowPromo(
    const FeaturePromoParams& params) const {
  auto* const spec = registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    return FeaturePromoResult::kError;
  }
  auto result = private_->queues.CanShow(*spec, params);
  if (result && !private_->tracker_->WouldTriggerHelpUI(*params.feature)) {
    // This can happen if a non-IPH promo is showing.
    // These are strongly advised against but they can happen.
    result = FeaturePromoResult::kBlockedByConfig;
  }
  return result;
}

void FeaturePromoController25::MaybeShowStartupPromo(
    FeaturePromoParams params) {
  MaybeShowPromo(std::move(params));
}

void FeaturePromoController25::MaybeShowPromo(FeaturePromoParams params) {
  auto* const spec = registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    PostShowPromoResult(std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kError);
    return;
  }

  if (current_promo() &&
      current_promo()->iph_feature() == &params.feature.get()) {
    PostShowPromoResult(std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kAlreadyQueued);
    return;
  }

  params.show_promo_result_callback = base::BindOnce(
      [](base::WeakPtr<FeaturePromoController25> controller,
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

  private_->queues.TryToQueue(*spec, std::move(params));
  MaybePostUpdate();
}

void FeaturePromoController25::MaybeShowPromoForDemoPage(
    FeaturePromoParams params) {
  auto* const spec = registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    PostShowPromoResult(std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kError);
    return;
  }

  if (auto* const feature = GetCurrentPromoFeature()) {
    // Note that overriding for promo or precedence doesn't trigger an update.
    EndPromo(*feature, FeaturePromoClosedReason::kOverrideForDemo);
  }
  private_->demo_queue.FailAll(FeaturePromoResult::kCanceled);
  private_->demo_queue.TryToQueue(*spec, std::move(params));

  // This can happen immediately since the message to show the promo will come
  // in on a (mostly) fresh call stack.
  UpdateQueuesAndMaybeShowPromo();
}

bool FeaturePromoController25::IsPromoQueued(
    const base::Feature& iph_feature) const {
  return private_->demo_queue.IsQueued(iph_feature) ||
         private_->queues.IsQueued(iph_feature);
}

bool FeaturePromoController25::MaybeUnqueuePromo(
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

void FeaturePromoController25::MaybeShowQueuedPromo() {
  // These are typically called when another promo ends, so update on a fresh
  // call stack.
  MaybePostUpdate();
}

base::WeakPtr<FeaturePromoController> FeaturePromoController25::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<FeaturePromoControllerCommon>
FeaturePromoController25::GetCommonWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FeaturePromoController25::IsDemoPending() const {
  return !private_->demo_queue.is_empty();
}

bool FeaturePromoController25::IsPromoQueued() const {
  return !private_->queues.IsEmpty();
}

FeaturePromoController25::PromoData
FeaturePromoController25::GetNextPromoData() {
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

FeaturePromoResult FeaturePromoController25::ShowPromo(PromoData& promo_data) {
  auto* const spec = registry()->GetParamsForFeature(promo_data.GetFeature());
  CHECK(spec);
  auto* const anchor_element = promo_data.GetAnchorElement();
  CHECK(anchor_element);
  auto& lifecycle = promo_data.GetLifecycle();
  const base::Feature& feature = promo_data.GetFeature();
  const bool in_demo_mode =
      (GetFeatureEngagementDemoFeatureName() == feature.name);
  auto* const index = internal::PreconditionData::Get(
      promo_data.eligible_promo->cached_data,
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
  auto bubble = ShowPromoBubbleImpl(std::move(build_params));
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

void FeaturePromoController25::MaybePostUpdate() {
  if (update_pending_ || poller_.IsRunning()) {
    return;
  }
  update_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FeaturePromoController25::UpdateQueuesAndMaybeShowPromo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FeaturePromoController25::UpdateQueuesAndMaybeShowPromo() {
  using PromoState = internal::MessagingCoordinator::PromoState;
  update_pending_ = false;

  // Fetch the next promo to be shown, or promo waiting to be shown.
  PromoData promo_data = GetNextPromoData();
  if (promo_data.eligible_promo) {
    const auto result = ShowPromo(promo_data);

    // Need to send the result regardless of success or failure.
    PostShowPromoResult(
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

void FeaturePromoController25::UpdatePollingState() {
  const bool should_poll =
      !private_->demo_queue.is_empty() || !private_->queues.IsEmpty();
  if (poller_.IsRunning()) {
    if (!should_poll) {
      poller_.Stop();
    }
  } else if (should_poll) {
    poller_.Start(FROM_HERE, features::GetPromoControllerPollingInterval(),
                  base::BindRepeating(
                      &FeaturePromoController25::UpdateQueuesAndMaybeShowPromo,
                      weak_ptr_factory_.GetWeakPtr()));
  }
}

void FeaturePromoController25::OnMessagingPriority() {
  // This is always called on a fresh stack so no need to post the update.
  UpdateQueuesAndMaybeShowPromo();
}

void FeaturePromoController25::OnPromoPreempted() {
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

void FeaturePromoController25::OnDestroying() {
  poller_.Stop();
  private_.reset();
}

// Default precondition providers are all created down here.

void FeaturePromoController25::AddDemoPreconditionProviders(
    ComposingPreconditionListProvider& to_add_to,
    bool required) {
  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoController25> controller,
           const FeaturePromoSpecification& spec,
           const FeaturePromoParams& params) {
          FeaturePromoPreconditionList list;
          if (auto* const ptr = controller.get()) {
            list.AddPrecondition(std::make_unique<LifecyclePrecondition>(
                ptr->CreateLifecycleFor(spec, params), /*for_demo=*/true));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoController25> controller,
           const FeaturePromoSpecification& spec, const FeaturePromoParams&) {
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
                spec, ptr->GetAnchorContext(), pre_increment));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  }
}

void FeaturePromoController25::AddPreconditionProviders(
    ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoController25> controller,
           const FeaturePromoSpecification& spec,
           const FeaturePromoParams& params) {
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
            // Finally, verify that the promo is eligible to show based on
            // session policy.
            list.AddPrecondition(std::make_unique<SessionPolicyPrecondition>(
                ptr->session_policy(),
                ptr->session_policy()->GetPromoPriorityInfo(spec),
                base::BindRepeating([]() {
                  return std::optional<
                      FeaturePromoPriorityProvider::PromoPriorityInfo>();
                })));
          }
          return list;
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<FeaturePromoController25> controller,
           const FeaturePromoSpecification& spec, const FeaturePromoParams&) {
          FeaturePromoPreconditionList list;
          if (auto* const ptr = controller.get()) {
            list.AddPrecondition(
                std::make_unique<ForwardingFeaturePromoPrecondition>(
                    ptr->private_->get_tracker_precondition()));
            list.AddPrecondition(std::make_unique<AnchorElementPrecondition>(
                spec, controller->GetAnchorContext(), false));
            // Wait-for state *does* take the current promo into account, since
            // a higher-weight promo might block a lower-weight promo.
            list.AddPrecondition(std::make_unique<SessionPolicyPrecondition>(
                ptr->session_policy(),
                ptr->session_policy()->GetPromoPriorityInfo(spec),
                base::BindRepeating(
                    [](base::WeakPtr<FeaturePromoController25> controller) {
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

}  // namespace user_education
