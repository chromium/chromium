// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_H_

#include <list>
#include <map>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/interaction/typed_data_collection.h"

namespace user_education::internal {

// Standard data for promos queued at a particular priority.
struct QueuedFeaturePromo {
  QueuedFeaturePromo(FeaturePromoParams params_,
                     UserEducationContextPtr context_,
                     FeaturePromoPreconditionList required_,
                     FeaturePromoPreconditionList wait_for_,
                     base::Time queue_time_);
  QueuedFeaturePromo(QueuedFeaturePromo&&) noexcept;
  ~QueuedFeaturePromo();

  FeaturePromoParams params;
  UserEducationContextPtr context;
  FeaturePromoPreconditionList required_preconditions;
  FeaturePromoPreconditionList wait_for_preconditions;
  base::Time queue_time;
};

// Represents information about a promo that is ready to show.
struct EligibleFeaturePromo {
  explicit EligibleFeaturePromo(FeaturePromoParams promo_params_,
                                UserEducationContextPtr promo_context);
  EligibleFeaturePromo(EligibleFeaturePromo&&) noexcept;
  EligibleFeaturePromo& operator=(EligibleFeaturePromo&&) noexcept;
  ~EligibleFeaturePromo();

  // The params for the promo that will be shown.
  FeaturePromoParams promo_params;

  // The context for the promo.
  UserEducationContextPtr promo_context;

  // The cached data from the preconditions that were satisfied in order for the
  // promo to show. This preserves the data from the queue when the promo is
  // removed.
  //
  // These are guaranteed to be current as of the promo being popped from the
  // queue, since all preconditions will have to be evaluated before a promo can
  // be returned by any of the `UpdateAnd...()` methods.
  ui::OwnedTypedDataCollection cached_data;
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

  // Returns whether the feature described by `spec` could be queued with
  // `promo_params`. Potentially as expensive as actually queueing the promo,
  // so use with care.
  FeaturePromoResult CanQueue(const FeaturePromoSpecification& spec,
                              const FeaturePromoParams& promo_params,
                              const UserEducationContextPtr& context) const;

  // Returns whether the feature described by `spec` could be shown immediately
  // `promo_params`. Potentially more expensive than actually queueing the
  // promo, so use with extreme care.
  FeaturePromoResult CanShow(const FeaturePromoSpecification& spec,
                             const FeaturePromoParams& promo_params,
                             const UserEducationContextPtr& context) const;

  // Attempts to queue a new promo defined by `spec` with `promo_params`. If
  // queueing the promo fails, for any reason the "show promo result" callback
  // will be posted with an appropriate failure code and the promo discarded.
  void TryToQueue(const FeaturePromoSpecification& spec,
                  FeaturePromoParams promo_params,
                  UserEducationContextPtr context);

  // Cancels promo for `iph_feature` if it is queued; returns whether it was
  // canceled.
  bool Cancel(const base::Feature& iph_feature);

  // Pops an eligible feature off the queue - usually the result of calling
  // `UpdateAndIdentifyNextEligiblePromo()`.
  EligibleFeaturePromo UnqueueEligiblePromo(const base::Feature& iph_feature);

  // Removes any ineligible promos from the queue and then returns the feature
  // associated with the next entry that is eligible to show, or null if none is
  // found.
  //
  // Unlike `UpdateAndGetNextEligiblePromo()`, does not remove the entry from
  // the queue. Use when you want to determine the next eligible promo without
  // actually showing it right away.
  //
  // Implicitly calls `RemoveIneligiblePromos()` as part of the initial cleanup
  // process.
  const base::Feature* UpdateAndIdentifyNextEligiblePromo();

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
  using ComputedDataMap =
      std::map<const base::Feature*, ui::UnownedTypedDataCollection>;

  // Posts the failure report to result_callback if it is valid.
  static void SendFailureReport(
      FeaturePromoController::ShowPromoResultCallback result_callback,
      FeaturePromoResult::Failure failure);

  // Removes promos that fail their required preconditions from the list,
  // calling their "show promo result" callbacks with the failure code of the
  // first failed required precondition.
  //
  // Populates and returns data for remaining promos.
  ComputedDataMap RemovePromosWithFailedPreconditions();

  // Removes timed-out promos from the list, calling their "show promo result"
  // callbacks with an appropriate failure code.
  //
  // Updates `data` and removes entries for deleted promos.
  void RemoveTimedOutPromos(ComputedDataMap& data);

  // Finds the first promo whose wait-for preconditions are met without popping
  // it from the queue. Should be called after the two "Remove..." methods above
  // to ensure only valid promos are returned.
  //
  // Uses `data` to pre-populate
  const base::Feature* IdentifyNextEligiblePromo(ComputedDataMap& data);

  // Returns an iterator to the queued promo for `feature`, or
  // `queued_promos_.end()` if not found.
  Queue::iterator FindQueuedPromo(const base::Feature& feature);

  // Records time a promo spent in queue and whether it succeeded.
  void RecordQueueTime(const internal::QueuedFeaturePromo& promo,
                       bool succeeded);

  Queue queued_promos_;

  raw_ref<const PreconditionListProvider> required_preconditions_provider_;
  raw_ref<const PreconditionListProvider> wait_for_preconditions_provider_;
  raw_ref<const UserEducationTimeProvider> time_provider_;
  base::TimeDelta queue_timeout_;
};

}  // namespace user_education::internal

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_H_
