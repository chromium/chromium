// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_controller_20.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace user_education {

namespace {
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kFeaturePromoControllerNotice);
}

struct FeaturePromoController20::QueuedPromoData {
  using PromoInfo = FeaturePromoPriorityProvider::PromoPriorityInfo;

  QueuedPromoData(FeaturePromoParams params_, PromoInfo info_)
      : params(std::move(params_)), info(info_) {}
  QueuedPromoData(QueuedPromoData&& other) noexcept = default;
  ~QueuedPromoData() = default;

  FeaturePromoParams params;
  PromoInfo info;
};

FeaturePromoController20::FeaturePromoController20(
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
                                   tutorial_service,
                                   messaging_controller) {}

FeaturePromoController20::~FeaturePromoController20() {
  FailQueuedPromos();
}

void FeaturePromoController20::MaybeShowStartupPromo(
    FeaturePromoParams params) {
  const base::Feature* const iph_feature = &params.feature.get();

  // No point in queueing a disabled feature.
  if (!in_iph_demo_mode() && !base::FeatureList::IsEnabled(*iph_feature)) {
    RecordPromoNotShown(iph_feature->name,
                        FeaturePromoResult::kFeatureDisabled);
    PostShowPromoResult(std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kFeatureDisabled);
    return;
  }

  // If the promo is currently running or already queued, fail.
  if (GetCurrentPromoFeature() == iph_feature || IsPromoQueued(*iph_feature)) {
    PostShowPromoResult(std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kAlreadyQueued);
    return;
  }

  // Get the specification.
  const auto* spec = registry()->GetParamsForFeature(*iph_feature);
  if (!spec) {
    PostShowPromoResult(std::move(params.show_promo_result_callback),
                        FeaturePromoResult::kError);
    return;
  }

  // Queue the promo.
  queued_promos_.emplace_back(std::move(params),
                              session_policy()->GetPromoPriorityInfo(*spec));

  // This will fire immediately if the tracker is initialized.
  feature_engagement_tracker()->AddOnInitializedCallback(base::BindOnce(
      &FeaturePromoController20::OnFeatureEngagementTrackerInitialized,
      weak_ptr_factory_.GetWeakPtr()));
}

FeaturePromoResult FeaturePromoController20::CanShowPromoCommon(
    const FeaturePromoParams& params,
    ShowSource source,
    CanShowPromoOutputs* outputs) const {
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
  if (!for_demo && !in_iph_demo_mode() &&
      !base::FeatureList::IsEnabled(*params.feature)) {
    return FeaturePromoResult::kFeatureDisabled;
  }

  // Check the lifecycle, but only if not in demo mode. This is especially
  // important for snoozeable, app, and legal notice promos. This will determine
  // if the promo is even eligible to show.
  auto lifecycle = std::make_unique<FeaturePromoLifecycle>(
      storage_service(), params.key, &*params.feature, spec->promo_type(),
      spec->promo_subtype(), spec->rotating_promos().size());
  if (spec->reshow_delay()) {
    lifecycle->SetReshowPolicy(*spec->reshow_delay(), spec->max_show_count());
  }
  if (!for_demo && !in_iph_demo_mode()) {
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
       feature_engagement_tracker()->ListEvents(*params.feature)) {
    if (!config.comparator.MeetsCriteria(count)) {
      return FeaturePromoResult::kBlockedByConfig;
    }
  }
#endif

  // Figure out if there's already a promo being shown.
  std::optional<FeaturePromoPriorityProvider::PromoPriorityInfo> current;
  if (current_promo()) {
    current = last_promo_info();
  } else if (bubble_factory_registry()->is_any_bubble_showing()) {
    current = FeaturePromoPriorityProvider::PromoPriorityInfo();
  }

  // When not in demo mode, refer to the session policy to determine if the
  // promo can show.
  if (!for_demo && !in_iph_demo_mode()) {
    const auto promo_info = session_policy()->GetPromoPriorityInfo(*spec);
    auto result = session_policy()->CanShowPromo(promo_info, current);
    if (!result) {
      return result;
    }

    // If this is not from the queue, compare against queued promos as well.
    if (source != ShowSource::kQueue) {
      if (const auto* const queued = GetNextQueuedPromo()) {
        // This is the opposite situation: only exclude this promo if the queued
        // promo (which is not yet running) would cancel *this* promo.
        result = session_policy()->CanShowPromo(queued->info, promo_info);
        if (result) {
          return FeaturePromoResult::kBlockedByPromo;
        }
      }
    }
  }

  // Promos are blocked if some other critical user messaging is queued.
  if (messaging_controller()->has_pending_notices() &&
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
    if (current_promo() && current_promo()->iph_feature() == &*params.feature) {
      current_index = (current_promo()->GetPromoIndex() + 1) %
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

  // Output additional information it was requested.
  if (outputs) {
    outputs->lifecycle = std::move(lifecycle);
    outputs->primary_spec = spec;
    outputs->display_spec = anchor_spec;
    outputs->anchor_element = anchor_element;
  }

  // Success - the promo can show.
  return FeaturePromoResult::Success();
}

void FeaturePromoController20::MaybeShowPromo(FeaturePromoParams params) {
  auto callback = std::move(params.show_promo_result_callback);
  PostShowPromoResult(
      std::move(callback),
      MaybeShowPromoImpl(std::move(params), ShowSource::kNormal));
}

FeaturePromoResult FeaturePromoController20::MaybeShowPromoForDemoPage(
    FeaturePromoParams params) {
  return MaybeShowPromoCommon(std::move(params), ShowSource::kDemo);
}

bool FeaturePromoController20::MaybeUnqueuePromo(
    const base::Feature& iph_feature) {
  const auto it = FindQueuedPromo(iph_feature);
  if (it != queued_promos_.end()) {
    auto& cb = it->params.show_promo_result_callback;
    if (cb) {
      std::move(cb).Run(FeaturePromoResult::kCanceled);
    }
    queued_promos_.erase(it);
    return true;
  }
  return false;
}

FeaturePromoController20::QueuedPromos::iterator
FeaturePromoController20::GetNextQueuedPromo() {
  QueuedPromos::iterator result = queued_promos_.end();
  for (auto it = queued_promos_.begin(); it != queued_promos_.end(); ++it) {
    if (result == queued_promos_.end() ||
        it->info.priority > result->info.priority) {
      result = it;
    }
  }
  return result;
}

const FeaturePromoController20::QueuedPromoData*
FeaturePromoController20::GetNextQueuedPromo() const {
  // Const cast is safe here because it does not modify the queue, and only a
  // const pointer to the value found is returned.
  const auto it =
      const_cast<FeaturePromoController20*>(this)->GetNextQueuedPromo();
  return it != queued_promos_.end() ? &*it : nullptr;
}

void FeaturePromoController20::MaybeShowQueuedPromo() {
  // This should only ever be called after the tracker is initialized.
  CHECK(feature_engagement_tracker()->IsInitialized());

  // If there is already a promo showing, it may be necessary to hold off trying
  // to show another.
  const std::optional<FeaturePromoPriorityProvider::PromoPriorityInfo> current =
      current_promo() ? std::make_optional(last_promo_info()) : std::nullopt;

  // Also, if the next promo in queue cannot be shown and the current promo is
  // not high-priority, any messaging priority must be released.
  const bool must_release_on_failure =
      !current ||
      current->priority != FeaturePromoSessionPolicy::PromoPriority::kHigh;

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
  if (current && !session_policy()->CanShowPromo(next->info, current)) {
    if (must_release_on_failure) {
      messaging_priority_handle_.Release();
    }
    return;
  }

  const bool is_high_priority =
      next->info.priority == FeaturePromoSessionPolicy::PromoPriority::kHigh;

  // Coordinate with the product messaging system to make sure a promo will not
  // attempt to be shown over a non-IPH legal notice.
  if (messaging_controller()->has_pending_notices()) {
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
      if (messaging_controller()->has_pending_notices()) {
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

FeaturePromoResult FeaturePromoController20::MaybeShowPromoImpl(
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
// Returns whether `iph_feature` is queued to be shown.
bool FeaturePromoController20::IsPromoQueued(
    const base::Feature& iph_feature) const {
  return std::any_of(queued_promos_.begin(), queued_promos_.end(),
                     [&iph_feature](const QueuedPromoData& data) {
                       return &data.params.feature.get() == &iph_feature;
                     });
}

// Returns an iterator into the queued promo list matching `iph_feature`, or
// `queued_promos_.end()` if not found.
FeaturePromoController20::QueuedPromos::iterator
FeaturePromoController20::FindQueuedPromo(const base::Feature& iph_feature) {
  return std::find_if(queued_promos_.begin(), queued_promos_.end(),
                      [&iph_feature](const QueuedPromoData& data) {
                        return &data.params.feature.get() == &iph_feature;
                      });
}

void FeaturePromoController20::OnFeatureEngagementTrackerInitialized(
    bool tracker_initialized_successfully) {
  if (tracker_initialized_successfully) {
    MaybeShowQueuedPromo();
  } else {
    FailQueuedPromos();
  }
}

void FeaturePromoController20::FailQueuedPromos() {
  for (auto& data : queued_promos_) {
    auto& cb = data.params.show_promo_result_callback;
    if (cb) {
      std::move(cb).Run(FeaturePromoResult::kError);
    }
  }
  queued_promos_.clear();
}

void FeaturePromoController20::MaybeRequestMessagePriority() {
  if (!messaging_controller()->IsNoticeQueued(kFeaturePromoControllerNotice)) {
    // Queues a request to be notified when all other notices have been
    // processed. This prevents the promo controller from immediately being
    // given priority again.
    messaging_controller()->QueueRequiredNotice(
        kFeaturePromoControllerNotice,
        base::BindOnce(&FeaturePromoController20::OnMessagePriority,
                       weak_ptr_factory_.GetWeakPtr()),
        {internal::kShowAfterAllNotices});
  }
}

void FeaturePromoController20::OnMessagePriority(
    RequiredNoticePriorityHandle notice_handle) {
  messaging_priority_handle_ = std::move(notice_handle);
  MaybeShowQueuedPromo();
}

base::WeakPtr<FeaturePromoController> FeaturePromoController20::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<FeaturePromoControllerCommon>
FeaturePromoController20::GetCommonWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace user_education
