// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_queue_set.h"

#include "base/containers/map_util.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_queue.h"

namespace user_education::internal {

FeaturePromoQueueSet::FeaturePromoQueueSet(
    const FeaturePromoPriorityProvider& priority_provider,
    const UserEducationTimeProvider& time_provider)
    : priority_provider_(priority_provider), time_provider_(time_provider) {}
FeaturePromoQueueSet::FeaturePromoQueueSet(FeaturePromoQueueSet&&) noexcept =
    default;
FeaturePromoQueueSet& FeaturePromoQueueSet::operator=(
    FeaturePromoQueueSet&&) noexcept = default;
FeaturePromoQueueSet::~FeaturePromoQueueSet() = default;

void FeaturePromoQueueSet::AddQueue(
    Priority priority,
    const PreconditionListProvider& required_preconditions_provider,
    const PreconditionListProvider& wait_for_preconditions_provider,
    base::TimeDelta queue_timeout) {
  const auto result = queues_.emplace(
      priority, FeaturePromoQueue(required_preconditions_provider,
                                  wait_for_preconditions_provider,
                                  *time_provider_, queue_timeout));
  CHECK(result.second);
}

bool FeaturePromoQueueSet::IsEmpty() const {
  for (auto& [pri, queue] : queues_) {
    if (!queue.is_empty()) {
      return false;
    }
  }
  return true;
}

size_t FeaturePromoQueueSet::GetTotalQueuedCount() const {
  size_t count = 0;
  for (auto& [pri, queue] : queues_) {
    count += queue.queued_count();
  }
  return count;
}

size_t FeaturePromoQueueSet::GetQueuedCount(Priority priority) const {
  auto* const queue = base::FindOrNull(queues_, priority);
  return queue ? queue->queued_count() : 0U;
}

bool FeaturePromoQueueSet::IsQueued(const base::Feature& iph_feature) const {
  for (const auto& [pri, queue] : queues_) {
    if (queue.IsQueued(iph_feature)) {
      return true;
    }
  }
  return false;
}

void FeaturePromoQueueSet::TryToQueue(const FeaturePromoSpecification& spec,
                                      FeaturePromoParams promo_params) {
  const auto info = priority_provider_->GetPromoPriorityInfo(spec);
  auto* const queue = base::FindOrNull(queues_, info.priority);
  if (!queue) {
    FeaturePromoQueue::SendFailureReport(
        std::move(promo_params.show_promo_result_callback),
        FeaturePromoResult::kError);
  } else {
    queue->TryToQueue(spec, std::move(promo_params));
  }
}

bool FeaturePromoQueueSet::Cancel(const base::Feature& iph_feature) {
  for (auto& [pri, queue] : queues_) {
    if (queue.Cancel(iph_feature)) {
      return true;
    }
  }
  return false;
}

std::optional<FeaturePromoParams>
FeaturePromoQueueSet::UpdateAndGetNextEligiblePromo() {
  std::optional<FeaturePromoParams> result;

  // Check queues from higher to lower priority. If an eligible promo is found,
  // or a promo remains waiting in a higher-priority queue, do not check lower-
  // priority queues; instead just evict ineligible promos.
  bool higher_priority_promo_found = false;
  for (auto& [pri, queue] : queues_) {
    if (higher_priority_promo_found) {
      queue.RemoveIneligiblePromos();
    } else {
      result = queue.UpdateAndGetNextEligiblePromo();
      higher_priority_promo_found = result.has_value() || !queue.is_empty();
    }
  }
  return result;
}

void FeaturePromoQueueSet::FailAll(FeaturePromoResult::Failure failure_reason) {
  for (auto& [pri, queue] : queues_) {
    queue.FailAll(failure_reason);
  }
}

}  // namespace user_education::internal
