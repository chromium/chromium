// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_queue.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace user_education::internal {

QueuedFeaturePromo::QueuedFeaturePromo(FeaturePromoParams params_,
                                       FeaturePromoPreconditionList required_,
                                       FeaturePromoPreconditionList wait_for_,
                                       base::Time queue_time_)
    : params(std::move(params_)),
      required_preconditions(std::move(required_)),
      wait_for_preconditions(std::move(wait_for_)),
      queue_time(queue_time_) {}
QueuedFeaturePromo::QueuedFeaturePromo(QueuedFeaturePromo&&) noexcept = default;
QueuedFeaturePromo::~QueuedFeaturePromo() = default;

EligibleFeaturePromo::EligibleFeaturePromo(FeaturePromoParams promo_params_)
    : promo_params(std::move(promo_params_)) {}
EligibleFeaturePromo::EligibleFeaturePromo(EligibleFeaturePromo&&) noexcept =
    default;
EligibleFeaturePromo& EligibleFeaturePromo::operator=(
    EligibleFeaturePromo&&) noexcept = default;
EligibleFeaturePromo::~EligibleFeaturePromo() = default;

FeaturePromoQueue::FeaturePromoQueue(
    const PreconditionListProvider& required_preconditions_provider,
    const PreconditionListProvider& wait_for_preconditions_provider,
    const UserEducationTimeProvider& time_provider,
    const base::TimeDelta queue_timeout)
    : required_preconditions_provider_(required_preconditions_provider),
      wait_for_preconditions_provider_(wait_for_preconditions_provider),
      time_provider_(time_provider),
      queue_timeout_(queue_timeout) {}

FeaturePromoQueue::FeaturePromoQueue(FeaturePromoQueue&&) noexcept = default;
FeaturePromoQueue& FeaturePromoQueue::operator=(FeaturePromoQueue&&) noexcept =
    default;
FeaturePromoQueue::~FeaturePromoQueue() = default;

bool FeaturePromoQueue::IsQueued(const base::Feature& iph_feature) const {
  return std::find_if(queued_promos_.begin(), queued_promos_.end(),
                      [&iph_feature](auto& entry) {
                        return &iph_feature == &*entry.params.feature;
                      }) != queued_promos_.end();
}

FeaturePromoResult FeaturePromoQueue::CanQueue(
    const FeaturePromoSpecification& spec,
    const FeaturePromoParams& promo_params) const {
  auto required =
      required_preconditions_provider_->GetPreconditions(spec, promo_params);
  ComputedData data;
  return required.CheckPreconditions(data).result();
}

FeaturePromoResult FeaturePromoQueue::CanShow(
    const FeaturePromoSpecification& spec,
    const FeaturePromoParams& promo_params) const {
  auto required =
      required_preconditions_provider_->GetPreconditions(spec, promo_params);
  ComputedData data;
  auto result = required.CheckPreconditions(data).result();
  if (!result) {
    return result;
  }
  auto wait_for =
      wait_for_preconditions_provider_->GetPreconditions(spec, promo_params);
  result = wait_for.CheckPreconditions(data).result();
  // Release references to data before the precondition lists go away.
  data.release_all_references();
  return result;
}

void FeaturePromoQueue::TryToQueue(const FeaturePromoSpecification& spec,
                                   FeaturePromoParams promo_params) {
  auto required =
      required_preconditions_provider_->GetPreconditions(spec, promo_params);
  ComputedData data;
  const auto required_check_result = required.CheckPreconditions(data);
  if (!required_check_result) {
    SendFailureReport(std::move(promo_params.show_promo_result_callback),
                      *required_check_result.failure());
    return;
  }

  const auto it = FindQueuedPromo(*spec.feature());
  if (it != queued_promos_.end()) {
    SendFailureReport(std::move(promo_params.show_promo_result_callback),
                      FeaturePromoResult::kAlreadyQueued);
    return;
  }

  queued_promos_.emplace_back(
      std::move(promo_params), std::move(required),
      wait_for_preconditions_provider_->GetPreconditions(spec, promo_params),
      time_provider_->GetCurrentTime());
}

bool FeaturePromoQueue::Cancel(const base::Feature& iph_feature) {
  const auto it = FindQueuedPromo(iph_feature);
  if (it == queued_promos_.end()) {
    return false;
  }
  SendFailureReport(std::move(it->params.show_promo_result_callback),
                    FeaturePromoResult::kCanceled);
  queued_promos_.erase(it);
  return true;
}

const base::Feature* FeaturePromoQueue::UpdateAndIdentifyNextEligiblePromo() {
  ComputedDataMap data = RemovePromosWithFailedPreconditions();
  RemoveTimedOutPromos(data);
  return IdentifyNextEligiblePromo(data);
}

EligibleFeaturePromo FeaturePromoQueue::UnqueueEligiblePromo(
    const base::Feature& iph_feature) {
  const auto it = FindQueuedPromo(iph_feature);
  CHECK(it != queued_promos_.end());
  RecordQueueTime(*it, /*succeeded=*/true);
  EligibleFeaturePromo eligible_promo(std::move(it->params));
  it->required_preconditions.ExtractCachedData(eligible_promo.cached_data);
  it->wait_for_preconditions.ExtractCachedData(eligible_promo.cached_data);
  queued_promos_.erase(it);
  return eligible_promo;
}

void FeaturePromoQueue::RemoveIneligiblePromos() {
  ComputedDataMap data = RemovePromosWithFailedPreconditions();
  RemoveTimedOutPromos(data);
}

void FeaturePromoQueue::FailAll(FeaturePromoResult::Failure failure_reason) {
  for (auto& promo : queued_promos_) {
    SendFailureReport(std::move(promo.params.show_promo_result_callback),
                      failure_reason);
  }
  queued_promos_.clear();
}

// static
void FeaturePromoQueue::SendFailureReport(
    FeaturePromoController::ShowPromoResultCallback result_callback,
    FeaturePromoResult::Failure failure) {
  if (result_callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FeaturePromoController::ShowPromoResultCallback result_callback,
               FeaturePromoResult::Failure failure) {
              std::move(result_callback).Run(failure);
            },
            std::move(result_callback), failure));
  }
}

FeaturePromoQueue::ComputedDataMap
FeaturePromoQueue::RemovePromosWithFailedPreconditions() {
  ComputedDataMap data;
  for (auto it = queued_promos_.begin(); it != queued_promos_.end();) {
    ComputedData temp;
    const auto check_result =
        it->required_preconditions.CheckPreconditions(temp);
    if (!check_result) {
      temp.release_all_references();
      RecordQueueTime(*it, /*succeeded=*/false);
      SendFailureReport(std::move(it->params.show_promo_result_callback),
                        *check_result.failure());
      it = queued_promos_.erase(it);
    } else {
      data.emplace(&it->params.feature.get(), std::move(temp));
      ++it;
    }
  }
  return data;
}

void FeaturePromoQueue::RemoveTimedOutPromos(ComputedDataMap& data) {
  const auto now = time_provider_->GetCurrentTime();
  for (auto it = queued_promos_.begin(); it != queued_promos_.end();) {
    if (now - it->queue_time >= queue_timeout_) {
      // The promo is expired, now to find out why.
      const auto temp = data.find(&it->params.feature.get());
      CHECK(temp != data.end());
      const auto latest_result =
          it->wait_for_preconditions.CheckPreconditions(temp->second);
      data.erase(temp);
      // If there was no identifiable reason, fall back to "timed out".
      RecordQueueTime(*it, /*succeeded=*/false);
      SendFailureReport(
          std::move(it->params.show_promo_result_callback),
          latest_result.failure().value_or(FeaturePromoResult::kTimedOut));
      it = queued_promos_.erase(it);
    } else {
      ++it;
    }
  }
}

const base::Feature* FeaturePromoQueue::IdentifyNextEligiblePromo(
    ComputedDataMap& data) {
  for (const auto& promo : queued_promos_) {
    const auto temp = data.find(&promo.params.feature.get());
    CHECK(temp != data.end());
    const auto result =
        promo.wait_for_preconditions.CheckPreconditions(temp->second);
    if (result) {
      return &promo.params.feature.get();
    }
  }
  return nullptr;
}

FeaturePromoQueue::Queue::iterator FeaturePromoQueue::FindQueuedPromo(
    const base::Feature& feature) {
  return std::find_if(
      queued_promos_.begin(), queued_promos_.end(),
      [&feature](auto& entry) { return &feature == &*entry.params.feature; });
}

void FeaturePromoQueue::RecordQueueTime(
    const internal::QueuedFeaturePromo& promo,
    bool succeeded) {
  std::string name = succeeded ? "UserEducation.MessageShown.TimeInQueue"
                               : "UserEducation.MessageNotShown.TimeInQueue";

  // Record generic queue time histogram (all promos).
  base::UmaHistogramCustomTimes(
      name, time_provider_->GetCurrentTime() - promo.queue_time,
      base::Milliseconds(250), base::Seconds(30), 50);

  // Record promo-specific queue time histogram.
  name += ".";
  name += promo.params.feature->name;
  base::UmaHistogramCustomTimes(
      name, time_provider_->GetCurrentTime() - promo.queue_time,
      base::Milliseconds(250), base::Seconds(30), 50);
}

}  // namespace user_education::internal
