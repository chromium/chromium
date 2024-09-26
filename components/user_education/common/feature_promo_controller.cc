// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_controller.h"

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_service.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kFeaturePromoControllerNotice);
}

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

struct FeaturePromoControllerCommon::ShowPromoBubbleParams {
  ShowPromoBubbleParams() = default;
  ShowPromoBubbleParams(ShowPromoBubbleParams&& other) noexcept = default;
  ~ShowPromoBubbleParams() = default;

  raw_ptr<const FeaturePromoSpecification> spec = nullptr;
  raw_ptr<ui::TrackedElement> anchor_element = nullptr;
  FeaturePromoSpecification::FormatParameters body_format;
  FeaturePromoSpecification::FormatParameters screen_reader_format;
  FeaturePromoSpecification::FormatParameters title_format;
  bool screen_reader_prompt_available = false;
  bool can_snooze = false;
};

struct FeaturePromoControllerCommon::QueuedPromoData {
  using PromoInfo = FeaturePromoSessionPolicy::PromoInfo;

  QueuedPromoData(FeaturePromoParams params_, PromoInfo info_)
      : params(std::move(params_)), info(info_) {}
  QueuedPromoData(QueuedPromoData&& other) noexcept = default;
  ~QueuedPromoData() = default;

  FeaturePromoParams params;
  PromoInfo info;
};

FeaturePromoControllerCommon::FeaturePromoControllerCommon(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    FeaturePromoStorageService* storage_service,
    FeaturePromoSessionPolicy* session_policy,
    TutorialService* tutorial_service,
    ProductMessagingController* messaging_controller)
    : in_iph_demo_mode_(
          base::FeatureList::IsEnabled(feature_engagement::kIPHDemoMode)),
      registry_(registry),
      feature_engagement_tracker_(feature_engagement_tracker),
      bubble_factory_registry_(help_bubble_registry),
      storage_service_(storage_service),
      session_policy_(session_policy),
      tutorial_service_(tutorial_service),
      messaging_controller_(messaging_controller) {
  DCHECK(feature_engagement_tracker_);
  DCHECK(bubble_factory_registry_);
  DCHECK(storage_service_);
}

FeaturePromoControllerCommon::~FeaturePromoControllerCommon() {
  FailQueuedPromos();
}

FeaturePromoResult FeaturePromoControllerCommon::CanShowPromo(
    const FeaturePromoParams& params) const {
  auto result = CanShowPromoCommon(params, ShowSource::kNormal);
  if (result &&
      !feature_engagement_tracker_->WouldTriggerHelpUI(*params.feature)) {
    result = FeaturePromoResult::kBlockedByConfig;
  }
  return result;
}

void FeaturePromoControllerCommon::MaybeShowPromo(FeaturePromoParams params) {
  auto callback = std::move(params.show_promo_result_callback);
  PostShowPromoResult(
      std::move(callback),
      MaybeShowPromoImpl(std::move(params), ShowSource::kNormal));
}

bool FeaturePromoControllerCommon::MaybeShowStartupPromo(
    FeaturePromoParams params) {
  const base::Feature* const iph_feature = &params.feature.get();

  // No point in queueing a disabled feature.
  if (!in_iph_demo_mode_ && !base::FeatureList::IsEnabled(*iph_feature)) {
    RecordPromoNotShown(iph_feature->name,
                        FeaturePromoResult::kFeatureDisabled);
    return false;
  }

  // If the promo is currently running, fail.
  if (GetCurrentPromoFeature() == iph_feature) {
    return false;
  }

  // If the promo is already queued, fail.
  if (IsPromoQueued(*iph_feature)) {
    return false;
  }

  // Queue the promo.
  const auto* spec = registry_->GetParamsForFeature(*iph_feature);
  if (!spec) {
    return false;
  }

  queued_promos_.emplace_back(std::move(params),
                              session_policy_->SpecificationToPromoInfo(*spec));

  // This will fire immediately if the tracker is initialized.
  feature_engagement_tracker_->AddOnInitializedCallback(base::BindOnce(
      &FeaturePromoControllerCommon::OnFeatureEngagementTrackerInitialized,
      weak_ptr_factory_.GetWeakPtr()));

  // The promo has been successfully queued. Once the FE backend is initialized,
  // MaybeShowPromo() will be called to see if the promo should actually be
  // shown.
  return true;
}

FeaturePromoResult FeaturePromoControllerCommon::MaybeShowPromoForDemoPage(
    FeaturePromoParams params) {
  return MaybeShowPromoCommon(std::move(params), ShowSource::kDemo);
}

FeaturePromoResult FeaturePromoControllerCommon::MaybeShowPromoImpl(
    FeaturePromoParams params,
    ShowSource source) {
  const char* feature_name = params.feature.get().name;
  auto result = MaybeShowPromoCommon(std::move(params), source);
  auto failure = result.failure();
  if (failure.has_value()) {
    RecordPromoNotShown(feature_name, failure.value());
  }
  return result;
}

FeaturePromoResult FeaturePromoControllerCommon::MaybeShowPromoCommon(
    FeaturePromoParams params,
    ShowSource source) {
  // Perform common checks.
  const FeaturePromoSpecification* primary_spec = nullptr;
  const FeaturePromoSpecification* display_spec = nullptr;
  std::unique_ptr<FeaturePromoLifecycle> lifecycle = nullptr;
  ui::TrackedElement* anchor_element = nullptr;
  auto result = CanShowPromoCommon(params, source, &primary_spec, &display_spec,
                                   &lifecycle, &anchor_element);
  if (!result) {
    return result;
  }
  CHECK(primary_spec);
  CHECK(display_spec);
  CHECK(lifecycle);
  CHECK(anchor_element);
  const bool for_demo = source == ShowSource::kDemo;

  // If the session policy allows overriding the current promo, abort it.
  if (current_promo_) {
    EndPromo(*GetCurrentPromoFeature(),
             for_demo ? FeaturePromoClosedReason::kOverrideForDemo
                      : FeaturePromoClosedReason::kOverrideForPrecedence);
  }

  // If the session policy allows overriding other help bubbles, close them.
  if (auto* const help_bubble =
          bubble_factory_registry_->GetHelpBubble(anchor_element->context())) {
    help_bubble->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
  }

  // TODO(crbug.com/40200981): Currently this must be called before
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
  show_params.spec = display_spec;
  show_params.anchor_element = anchor_element;
  show_params.screen_reader_prompt_available = screen_reader_available;
  show_params.body_format = std::move(params.body_params);
  show_params.screen_reader_format = std::move(params.screen_reader_params);
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
  last_promo_info_ = session_policy_->SpecificationToPromoInfo(*primary_spec);
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

FeaturePromoStatus FeaturePromoControllerCommon::GetPromoStatus(
    const base::Feature& iph_feature) const {
  if (IsPromoQueued(iph_feature)) {
    return FeaturePromoStatus::kQueuedForStartup;
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
  const auto it = FindQueuedPromo(iph_feature);
  if (it != queued_promos_.end()) {
    auto& cb = it->params.show_promo_result_callback;
    if (cb) {
      std::move(cb).Run(FeaturePromoResult::kCanceled);
    }
    queued_promos_.erase(it);
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
    feature_engagement_tracker()->NotifyUsedEvent(feature);
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

base::WeakPtr<FeaturePromoController>
FeaturePromoControllerCommon::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FeaturePromoControllerCommon::CheckScreenReaderPromptAvailable(
    bool for_demo) const {
  if (!ui::AXPlatform::GetInstance().GetMode().has_mode(
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

  // TODO(crbug.com/40200981): Once we have our answer, immediately dismiss
  // so that this doesn't interfere with actually showing the bubble. This
  // dismiss can be moved elsewhere once we support concurrency.
  feature_engagement_tracker_->Dismissed(*prompt_feature);

  return true;
}

void FeaturePromoControllerCommon::OnFeatureEngagementTrackerInitialized(
    bool tracker_initialized_successfully) {
  if (tracker_initialized_successfully) {
    MaybeShowQueuedPromo();
  } else {
    FailQueuedPromos();
  }
}

void FeaturePromoControllerCommon::MaybeRequestMessagePriority() {
  if (!messaging_controller_->IsNoticeQueued(kFeaturePromoControllerNotice)) {
    // Queues a request to be notified when all other notices have been
    // processed. This prevents the promo controller from immediately being
    // given priority again.
    messaging_controller_->QueueRequiredNotice(
        kFeaturePromoControllerNotice,
        base::BindOnce(&FeaturePromoControllerCommon::OnMessagePriority,
                       weak_ptr_factory_.GetWeakPtr()),
        {internal::kShowAfterAllNotices});
  }
}

void FeaturePromoControllerCommon::OnMessagePriority(
    RequiredNoticePriorityHandle notice_handle) {
  messaging_priority_handle_ = std::move(notice_handle);
  MaybeShowQueuedPromo();
}

FeaturePromoControllerCommon::QueuedPromos::iterator
FeaturePromoControllerCommon::GetNextQueuedPromo() {
  QueuedPromos::iterator result = queued_promos_.end();
  for (auto it = queued_promos_.begin(); it != queued_promos_.end(); ++it) {
    if (result == queued_promos_.end() ||
        it->info.priority > result->info.priority) {
      result = it;
    }
  }
  return result;
}

const FeaturePromoControllerCommon::QueuedPromoData*
FeaturePromoControllerCommon::GetNextQueuedPromo() const {
  // Const cast is safe here because it does not modify the queue, and only a
  // const pointer to the value found is returned.
  const auto it =
      const_cast<FeaturePromoControllerCommon*>(this)->GetNextQueuedPromo();
  return it != queued_promos_.end() ? &*it : nullptr;
}

void FeaturePromoControllerCommon::MaybeShowQueuedPromo() {
  // This should only ever be called after the tracker is initialized.
  CHECK(feature_engagement_tracker_->IsInitialized());

  // If there is already a promo showing, it may be necessary to hold off trying
  // to show another.
  const std::optional<FeaturePromoSessionPolicy::PromoInfo> current_promo =
      current_promo_ ? std::make_optional(last_promo_info_) : std::nullopt;

  // Also, if the next promo in queue cannot be shown and the current promo is
  // not high-priority, any messaging priority must be released.
  const bool must_release_on_failure =
      !current_promo || current_promo->priority !=
                            FeaturePromoSessionPolicy::PromoPriority::kHigh;

  // Fetch the next-highest-priority promo from the queue. If there's nothing,
  // then there's nothing to do.
  const auto next = GetNextQueuedPromo();
  if (next == queued_promos_.end()) {
    if (must_release_on_failure) {
      messaging_priority_handle_.Release();
    }
    return;
  }

  // If there is already a promo showing and this promo would not override it,
  // bail out.
  if (current_promo &&
      !session_policy_->CanShowPromo(next->info, current_promo)) {
    if (must_release_on_failure) {
      messaging_priority_handle_.Release();
    }
    return;
  }

  const bool is_high_priority =
      next->info.priority == FeaturePromoSessionPolicy::PromoPriority::kHigh;

  // Coordinate with the product messaging system to make sure a promo will not
  // attempt to be shown over a non-IPH legal notice.
  if (messaging_controller_->has_pending_notices()) {
    // Does the FeaturePromoController have messaging priority?
    if (!messaging_priority_handle_) {
      // No, which means another non-IPH promo does. Request priority and quit
      // for now.
      MaybeRequestMessagePriority();
      return;
    }

    // The controller has priority. Whether it keeps it depends on whether a
    // high-priority promo is queued; for high-priority promos, retain message
    // priority until all such promos are shown or fail to show.
    if (!is_high_priority) {
      // Not high-priority. Release the handle and see if there are any
      // additional pending non-IPH notices. This may show another notice, but
      // it will be deferred a frame.
      messaging_priority_handle_.Release();
      if (messaging_controller_->has_pending_notices()) {
        // Register again to be given priority after all remaining notices are
        // shown. This will not cause a race because the method below queues a
        // request that must be processed only after all other requests to show
        // critical messages have completed.
        MaybeRequestMessagePriority();
        return;
      }
    }
  }

  // It's possible that the promo controller doesn't have messaging priority but
  // wants to show a high-priority IPH. In that case, do not proceed until the
  // controller receives priority.
  if (is_high_priority && !messaging_priority_handle_) {
    MaybeRequestMessagePriority();
    return;
  }

  // At this point, the priority handle should be held if and only if the IPH to
  // be shown is high-priority. (This is a DCHECK because failing to satisfy
  // this constraint won't cause a crash, just potentially undesirable behavior
  // in a very unlikely corner case.)
  DCHECK_EQ(!!messaging_priority_handle_, is_high_priority);

  // Store the data that is needed to show the promo and then remove it from
  // the queue.
  FeaturePromoParams params = std::move(next->params);
  queued_promos_.erase(next);
  ShowPromoResultCallback callback =
      std::move(params.show_promo_result_callback);

  // Try to start the promo, assuming the tracker was successfully initialized.
  const FeaturePromoResult result =
      MaybeShowPromoImpl(std::move(params), ShowSource::kQueue);
  if (callback) {
    std::move(callback).Run(result);
  }

  // On failure, there may still be promos to show, so attempt to show the next
  // one in the queue (this method exits immediately if the queue is empty).
  if (!result) {
    MaybeShowQueuedPromo();
  }
}

// Returns whether `iph_feature` is queued to be shown.
bool FeaturePromoControllerCommon::IsPromoQueued(
    const base::Feature& iph_feature) const {
  return std::any_of(queued_promos_.begin(), queued_promos_.end(),
                     [&iph_feature](const QueuedPromoData& data) {
                       return &data.params.feature.get() == &iph_feature;
                     });
}

// Returns an iterator into the queued promo list matching `iph_feature`, or
// `queued_promos_.end()` if not found.
FeaturePromoControllerCommon::QueuedPromos::iterator
FeaturePromoControllerCommon::FindQueuedPromo(
    const base::Feature& iph_feature) {
  return std::find_if(queued_promos_.begin(), queued_promos_.end(),
                      [&iph_feature](const QueuedPromoData& data) {
                        return &data.params.feature.get() == &iph_feature;
                      });
}

void FeaturePromoControllerCommon::FailQueuedPromos() {
  for (auto& data : queued_promos_) {
    auto& cb = data.params.show_promo_result_callback;
    if (cb) {
      std::move(cb).Run(FeaturePromoResult::kError);
    }
  }
  queued_promos_.clear();
}

FeaturePromoResult FeaturePromoControllerCommon::CanShowPromoCommon(
    const FeaturePromoParams& params,
    ShowSource source,
    const FeaturePromoSpecification** primary_spec_out,
    const FeaturePromoSpecification** display_spec_out,
    std::unique_ptr<FeaturePromoLifecycle>* lifecycle_out,
    ui::TrackedElement** anchor_element_out) const {
  const bool for_demo = source == ShowSource::kDemo;

  // Ensure that this promo isn't already queued for startup.
  //
  // Note that this check is bypassed if this is for an explicit demo, but not
  // in demo mode, as the IPH may be queued for startup specifically because it
  // is being demoed.
  if (!for_demo && IsPromoQueued(*params.feature)) {
    return FeaturePromoResult::kBlockedByPromo;
  }

  const FeaturePromoSpecification* const spec =
      registry()->GetParamsForFeature(*params.feature);
  if (!spec) {
    return FeaturePromoResult::kError;
  }

  // When not bypassing the normal gating systems, don't try to show promos for
  // disabled features. This prevents us from calling into the Feature
  // Engagement tracker more times than necessary, emitting unnecessary logging
  // events when features are disabled.
  if (!for_demo && !in_iph_demo_mode_ &&
      !base::FeatureList::IsEnabled(*params.feature)) {
    return FeaturePromoResult::kFeatureDisabled;
  }

  // Check the lifecycle, but only if not in demo mode. This is especially
  // important for snoozeable, app, and legal notice promos. This will determine
  // if the promo is even eligible to show.
  auto lifecycle = std::make_unique<FeaturePromoLifecycle>(
      storage_service_, params.key, &*params.feature, spec->promo_type(),
      spec->promo_subtype(), spec->rotating_promos().size());
  if (spec->reshow_delay()) {
    lifecycle->SetReshowPolicy(*spec->reshow_delay(), spec->max_show_count());
  }
  if (!for_demo && !in_iph_demo_mode_) {
    if (const auto result = lifecycle->CanShow(); !result) {
      return result;
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  // Need to check that the Feature Engagement Tracker isn't blocking the
  // feature for event-based reasons (e.g. the feature was already used so
  // there's no need to promote it). This prevents us from allowing a promo to
  // preempt and close another promo or Tutorial because it passes all of the
  // checks, only to discover that it is blocked by the tracker config.
  for (const auto& [config, count] :
       feature_engagement_tracker_->ListEvents(*params.feature)) {
    if (!config.comparator.MeetsCriteria(count)) {
      return FeaturePromoResult::kBlockedByConfig;
    }
  }
#endif

  // Figure out if there's already a promo being shown.
  std::optional<FeaturePromoSessionPolicy::PromoInfo> current_promo;
  if (current_promo_) {
    current_promo = last_promo_info_;
  } else if (bubble_factory_registry_->is_any_bubble_showing()) {
    current_promo = FeaturePromoSessionPolicy::PromoInfo();
  }

  // When not in demo mode, refer to the session policy to determine if the
  // promo can show.
  if (!for_demo && !in_iph_demo_mode_) {
    const auto promo_info = session_policy_->SpecificationToPromoInfo(*spec);
    auto result = session_policy_->CanShowPromo(promo_info, current_promo);
    if (!result) {
      return result;
    }

    // If this is not from the queue, compare against queued promos as well.
    if (source != ShowSource::kQueue) {
      if (const auto* const queued = GetNextQueuedPromo()) {
        // This is the opposite situation: only exclude this promo if the queued
        // promo (which is not yet running) would cancel *this* promo.
        result = session_policy_->CanShowPromo(queued->info, promo_info);
        if (result) {
          return FeaturePromoResult::kBlockedByPromo;
        }
      }
    }
  }

  // Promos are blocked if some other critical user messaging is queued.
  if (messaging_controller_->has_pending_notices() &&
      !messaging_priority_handle_) {
    return FeaturePromoResult::kBlockedByPromo;
  }

  // For rotating promos, cycle forward to the next valid index.
  auto* anchor_spec = spec;
  if (spec->promo_type() == FeaturePromoSpecification::PromoType::kRotating) {
    int current_index = lifecycle->GetPromoIndex();
    // In demos, when repeating the same repeating promo to test it, the index
    // should cycle. However, the updated index is not written until the
    // previous promo is ended, which happens later. In order to simulate this,
    // base the starting index off the one being used by the previous promo.
    if (current_promo_ && current_promo_->iph_feature() == &*params.feature) {
      current_index = (current_promo_->GetPromoIndex() + 1) %
                      spec->rotating_promos().size();
    }

    // Find the next index in the rotation that has a valid promo. This is the
    // actual index that will be used.
    int index = current_index;
    while (!spec->rotating_promos().at(index).has_value()) {
      index = (index + 1) % spec->rotating_promos().size();
      CHECK_NE(index, current_index)
          << "Wrapped around while looking for a valid rotating promo; this "
             "should have been caught during promo registration.";
    }
    lifecycle->SetPromoIndex(index);
    anchor_spec = &*spec->rotating_promos().at(index);
  }

  // Fetch the anchor element. For now, assume all elements are Views.
  ui::TrackedElement* const anchor_element =
      anchor_spec->GetAnchorElement(GetAnchorContext());
  if (!anchor_element) {
    return FeaturePromoResult::kBlockedByUi;
  }

  // Some contexts and anchors are not appropriate for showing normal promos.
  if (!CanShowPromoForElement(anchor_element)) {
    return FeaturePromoResult::kBlockedByUi;
  }

  // Output the lifecycle if it was requested.
  if (lifecycle_out) {
    *lifecycle_out = std::move(lifecycle);
  }

  // If the caller has asked for the specification or anchor element, then
  // provide them.
  if (primary_spec_out) {
    *primary_spec_out = spec;
  }
  if (display_spec_out) {
    *display_spec_out = anchor_spec;
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
  if (spec.screen_reader_accelerator()) {
    CHECK(spec.screen_reader_string_id());
    CHECK(std::holds_alternative<FeaturePromoSpecification::NoSubstitution>(
        params.screen_reader_format))
        << "Accelerator and substitution are not compatible for screen "
           "reader text.";
    bubble_params.screenreader_text =
        l10n_util::GetStringFUTF16(spec.screen_reader_string_id(),
                                   spec.screen_reader_accelerator()
                                       .GetAccelerator(GetAcceleratorProvider())
                                       .GetShortcutText());
  } else {
    bubble_params.screenreader_text = FeaturePromoSpecification::FormatString(
        spec.screen_reader_string_id(), std::move(params.screen_reader_format));
  }
  bubble_params.close_button_alt_text =
      l10n_util::GetStringUTF16(IDS_CLOSE_PROMO);
  bubble_params.body_icon = spec.bubble_icon();
  if (spec.bubble_body_string_id())
    bubble_params.body_icon_alt_text = GetBodyIconAltText();
  bubble_params.arrow = spec.bubble_arrow();
  bubble_params.focus_on_show_hint = spec.focus_on_show_override();

  bubble_params.timeout_callback = base::BindOnce(
      &FeaturePromoControllerCommon::OnHelpBubbleTimeout,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()));

  // Feature isn't present for some critical promos.
  if (spec.feature()) {
    bubble_params.dismiss_callback = base::BindOnce(
        &FeaturePromoControllerCommon::OnHelpBubbleDismissed,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(spec.feature()),
        /* via_action_button =*/false);
  }

  switch (spec.promo_type()) {
    case FeaturePromoSpecification::PromoType::kToast: {
      // Rotating toast promos require a "got it" button.
      if (current_promo_ &&
          current_promo_->promo_type() ==
              FeaturePromoSpecification::PromoType::kRotating) {
        bubble_params.buttons = CreateRotatingToastButtons(*spec.feature());
        // If no hint is set, promos with buttons take focus. However, toasts do
        // not take focus by default. So if the hint isn't already set, set the
        // promo not to take focus.
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
    case FeaturePromoSpecification::PromoType::kLegacy:
      break;
    case FeaturePromoSpecification::PromoType::kRotating:
      NOTREACHED() << "Not implemented; should never reach this code.";
  }

  bool had_screen_reader_promo = false;
  if (spec.promo_type() == FeaturePromoSpecification::PromoType::kTutorial) {
    bubble_params.keyboard_navigation_hint = GetTutorialScreenReaderHint();
  } else if (params.screen_reader_prompt_available) {
    bubble_params.keyboard_navigation_hint = GetFocusHelpBubbleScreenReaderHint(
        spec.promo_type(), params.anchor_element);
    had_screen_reader_promo = !bubble_params.keyboard_navigation_hint.empty();
  }

  auto help_bubble = bubble_factory_registry_->CreateHelpBubble(
      params.anchor_element, std::move(bubble_params));
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
  if (bubble == promo_bubble()) {
    if (current_promo_->OnPromoBubbleClosed(reason)) {
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
  switch (failure) {
    case FeaturePromoResult::kCanceled:
      failure_action_name.append("Canceled");
      break;
    case FeaturePromoResult::kError:
      failure_action_name.append("Error");
      break;
    case FeaturePromoResult::kBlockedByUi:
      failure_action_name.append("BlockedByUi");
      break;
    case FeaturePromoResult::kBlockedByPromo:
      failure_action_name.append("BlockedByPromo");
      break;
    case FeaturePromoResult::kBlockedByConfig:
      failure_action_name.append("BlockedByConfig");
      break;
    case FeaturePromoResult::kSnoozed:
      failure_action_name.append("Snoozed");
      break;
    case FeaturePromoResult::kBlockedByContext:
      failure_action_name.append("BlockedByContext");
      break;
    case FeaturePromoResult::kFeatureDisabled:
      failure_action_name.append("FeatureDisabled");
      break;
    case FeaturePromoResult::kPermanentlyDismissed:
      failure_action_name.append("PermanentlyDismissed");
      break;
    case FeaturePromoResult::kBlockedByGracePeriod:
      failure_action_name.append("BlockedByGracePeriod");
      break;
    case FeaturePromoResult::kBlockedByCooldown:
      failure_action_name.append("BlockedByCooldown");
      break;
    case FeaturePromoResult::kRecentlyAborted:
      failure_action_name.append("RecentlyAborted");
      break;
    case FeaturePromoResult::kExceededMaxShowCount:
      failure_action_name.append("ExceededMaxShowCount");
      break;
    case FeaturePromoResult::kBlockedByNewProfile:
      failure_action_name.append("BlockedByNewProfile");
      break;
    case FeaturePromoResult::kBlockedByReshowDelay:
      failure_action_name.append("BlockedByReshowDelay");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
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
    case FeaturePromoStatus::kQueuedForStartup:
      os << "kQueuedForStartup";
      break;
  }
  return os;
}

}  // namespace user_education
