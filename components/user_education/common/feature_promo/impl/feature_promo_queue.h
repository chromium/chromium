// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_H_

#include <list>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education::internal {

// Standard data for promos queued at a particular priority.
struct QueuedFeaturePromo {
  QueuedFeaturePromo(FeaturePromoParams params_,
                     FeaturePromoPreconditionList required_,
                     FeaturePromoPreconditionList wait_for_,
                     base::Time queue_time_);
  QueuedFeaturePromo(QueuedFeaturePromo&&) noexcept;
  ~QueuedFeaturePromo();

  FeaturePromoParams params;
  FeaturePromoPreconditionList required_preconditions;
  FeaturePromoPreconditionList wait_for_preconditions;
  base::Time queue_time;
};

// Represents a queue of promos to be shown at a particular priority.
//
// Promos that pass their required preconditions are added to the queue, and are
// later removed if they time out or a required precondition subsequently fails.
//
// When a promo is removed from the queue (rather than being returned as the
// next eligible promo), its `FeaturePromoParams::show_promo_result_callback` is
// called with an appropriate failure reason. This happens on a fresh call stack
// via a post, so no callbacks will be made during calls into this object.
class FeaturePromoQueue {
 public:
  FeaturePromoQueue(
      const PreconditionListProvider& required_preconditions_provider,
      const PreconditionListProvider& wait_for_preconditions_provider,
      const UserEducationTimeProvider& time_provider,
      const base::TimeDelta queue_timeout);
  FeaturePromoQueue(FeaturePromoQueue&&) noexcept;
  FeaturePromoQueue& operator=(FeaturePromoQueue&&) noexcept;
  ~FeaturePromoQueue();

  bool is_empty() const { return queued_promos_.empty(); }
  size_t queued_count() const { return queued_promos_.size(); }

  // Returns whether the queue contains the given feature.
  bool IsQueued(const base::Feature& iph_feature) const;

  // Attempts to queue a new promo defined by `spec` with `promo_params`. If
  // queueing the promo fails, for any reason the "show promo result" callback
  // will be posted with an appropriate failure code and the promo discarded.
  void TryToQueue(const FeaturePromoSpecification& spec,
                  FeaturePromoParams promo_params);

  // Cancels promo for `iph_feature` if it is queued; returns whether it was
  // canceled.
  bool Cancel(const base::Feature& iph_feature);

  // Removes any ineligible promos from the queue and then returns the next
  // entry that is eligible to show, or null if none is found.
  //
  // Implicitly calls `RemoveIneligiblePromos()` as part of the initial cleanup
  // process.
  std::optional<FeaturePromoParams> UpdateAndGetNextEligiblePromo();

  // Removes timed-out and prohibited promos without trying to retrieve the next
  // eligible promo. "Show promo result" callbacks are called for any removed
  // entries. Use this for periodic cleanup if `UpdateAndGetNextEligiblePromo()`
  // isn't being called.
  void RemoveIneligiblePromos();

  // Fails all promos in the queue with the given `failure_reason`.
  void FailAll(FeaturePromoResult::Failure failure_reason);

 private:
  friend class FeaturePromoQueueSet;
  friend class FeaturePromoQueueTest;

  using Queue = std::list<internal::QueuedFeaturePromo>;

  // Posts the failure report to result_callback if it is valid.
  static void SendFailureReport(
      FeaturePromoController::ShowPromoResultCallback result_callback,
      FeaturePromoResult::Failure failure);

  // Removes timed-out promos from the list, calling their "show promo result"
  // callbacks with an appropriate failure code.
  void RemoveTimedOutPromos();

  // Removes promos that fail their required preconditions from the list,
  // calling their "show promo result" callbacks with the failure code of the
  // first failed required precondition.
  void RemovePromosWithFailedPreconditions();

  // Finds, pops, and returns the first promo whose wait-for preconditions are
  // met. Should be called after the two "Remove..." methods above to ensure
  // only valid promos are returned.
  std::optional<FeaturePromoParams> GetNextEligiblePromo();

  // Returns an iterator to the queued promo for `feature`, or
  // `queued_promos_.end()` if not found.
  Queue::iterator FindQueuedPromo(const base::Feature& feature);

  Queue queued_promos_;

  raw_ref<const PreconditionListProvider> required_preconditions_provider_;
  raw_ref<const PreconditionListProvider> wait_for_preconditions_provider_;
  raw_ref<const UserEducationTimeProvider> time_provider_;
  base::TimeDelta queue_timeout_;
};

}  // namespace user_education::internal

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_H_
