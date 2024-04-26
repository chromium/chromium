// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/queries.h"

#include <utility>

#include "base/check.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/timer/timer.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"

namespace resource_attribution {

namespace {

using QueryParams = internal::QueryParams;
using QueryScheduler = internal::QueryScheduler;

// The minimum delay between QueryOnce() calls for kMemorySummary resources.
// This can only be updated in unit tests so doesn't need to be thread-safe.
// Copied from ProcessMetricsDecorator::kMinImmediateRefreshDelay.
// TODO(crbug.com/40926264): Manage timing centrally in QueryScheduler.
base::TimeDelta g_min_memory_query_delay = base::Seconds(2);

void AddScopedQueryToScheduler(QueryParams* query_params,
                               QueryScheduler* scheduler) {
  scheduler->AddScopedQuery(query_params);
}

void RemoveScopedQueryFromScheduler(std::unique_ptr<QueryParams> query_params,
                                    QueryScheduler* scheduler) {
  scheduler->RemoveScopedQuery(std::move(query_params));
}

void StartRepeatingQueryInScheduler(QueryParams* query_params,
                                    QueryScheduler* scheduler) {
  scheduler->StartRepeatingQuery(query_params);
}

void RequestResultsFromScheduler(
    QueryParams* query_params,
    base::OnceCallback<void(const QueryResultMap&)> callback,
    QueryScheduler* scheduler) {
  scheduler->RequestResults(*query_params, std::move(callback));
}

}  // namespace

class ScopedResourceUsageQuery::ThrottledTimer {
 public:
  ThrottledTimer() = default;
  ~ThrottledTimer() = default;

  ThrottledTimer(const ThrottledTimer&) = delete;
  ThrottledTimer& operator=(const ThrottledTimer&) = delete;

  // Starts the timer to repeatedly query `params` after `delay`.
  // `observer_list` will be notified with the results.
  void StartTimer(base::TimeDelta delay,
                  internal::QueryParams* params,
                  scoped_refptr<ObserverList> observer_list);

  // Sends the scheduler a request for query results for `params`.
  // `observer_list` will be notified with the results. If `timer_fired` is
  // true, this is invoked from the timer, otherwise it's invoked from
  // QueryOnce().
  void SendRequestToScheduler(internal::QueryParams* params,
                              scoped_refptr<ObserverList> observer_list,
                              bool timer_fired);

 private:
  // Returns true if SendRequestToScheduler should be called for `params`, false
  // otherwise. This must be called on every request to update state.
  bool ShouldSendRequest(internal::QueryParams* params, bool timer_fired);

  base::RepeatingTimer timer_;
  base::TimeTicks last_fire_time_;
  base::TimeTicks next_fire_time_;
  base::TimeTicks last_query_once_time_;
};

void ScopedResourceUsageQuery::ThrottledTimer::StartTimer(
    base::TimeDelta delay,
    internal::QueryParams* params,
    scoped_refptr<ObserverList> observer_list) {
  CHECK(!timer_.IsRunning());
  CHECK(delay.is_positive());
  // Unretained is safe because ScopedResourceUsageQuery owns both `this` and
  // `params`.
  timer_.Start(FROM_HERE, delay,
               base::BindRepeating(&ThrottledTimer::SendRequestToScheduler,
                                   base::Unretained(this),
                                   base::Unretained(params), observer_list,
                                   /*timer_fired=*/true));
  next_fire_time_ = base::TimeTicks::Now() + delay;
}

void ScopedResourceUsageQuery::ThrottledTimer::SendRequestToScheduler(
    internal::QueryParams* params,
    scoped_refptr<ObserverList> observer_list,
    bool timer_fired) {
  if (ShouldSendRequest(params, timer_fired)) {
    // Unretained is safe because the ScopedResourceUsageQuery destructor passes
    // `params` to the scheduler sequence to delete.
    QueryScheduler::CallWithScheduler(base::BindOnce(
        &RequestResultsFromScheduler, base::Unretained(params),
        base::BindOnce(&ScopedResourceUsageQuery::NotifyObservers,
                       observer_list)));
  }
}

bool ScopedResourceUsageQuery::ThrottledTimer::ShouldSendRequest(
    internal::QueryParams* params,
    bool timer_fired) {
  if (!params->resource_types.Has(ResourceType::kMemorySummary)) {
    // Only memory queries are throttled.
    return true;
  }

  const auto now = base::TimeTicks::Now();
  if (timer_fired) {
    // Repeating queries aren't throttled, but need to save the current time to
    // throttle QueryOnce().
    CHECK(timer_.IsRunning());
    last_fire_time_ = now;
    next_fire_time_ = now + timer_.GetCurrentDelay();
    return true;
  }

  // Check if this QueryOnce() should be throttled.
  if (!last_query_once_time_.is_null() &&
      now < last_query_once_time_ + g_min_memory_query_delay) {
    // QueryOnce() called recently.
    return false;
  }
  if (!last_fire_time_.is_null() &&
      now < last_fire_time_ + g_min_memory_query_delay) {
    // Timer fired recently.
    return false;
  }
  if (!next_fire_time_.is_null() &&
      now > next_fire_time_ - g_min_memory_query_delay) {
    // Timer is going to fire soon.
    return false;
  }
  last_query_once_time_ = now;
  return true;
}

ScopedResourceUsageQuery::ScopedDisableMemoryQueryDelayForTesting::
    ScopedDisableMemoryQueryDelayForTesting()
    : previous_delay_(g_min_memory_query_delay) {
  g_min_memory_query_delay = base::TimeDelta();
}

ScopedResourceUsageQuery::ScopedDisableMemoryQueryDelayForTesting::
    ~ScopedDisableMemoryQueryDelayForTesting() {
  CHECK(g_min_memory_query_delay.is_zero());
  g_min_memory_query_delay = previous_delay_;
}

ScopedResourceUsageQuery::~ScopedResourceUsageQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!params_) {
    // `params_` was moved to another ScopedResourceUsageQuery.
    return;
  }
  // Notify the scheduler this query no longer exists. Sends the QueryParams to
  // the scheduler to delete to be sure they're valid until the scheduler reads
  // them.
  QueryScheduler::CallWithScheduler(
      base::BindOnce(&RemoveScopedQueryFromScheduler, std::move(params_)));
}

ScopedResourceUsageQuery::ScopedResourceUsageQuery(ScopedResourceUsageQuery&&) =
    default;

ScopedResourceUsageQuery& ScopedResourceUsageQuery::operator=(
    ScopedResourceUsageQuery&&) = default;

void ScopedResourceUsageQuery::AddObserver(QueryResultObserver* observer) {
  // ObserverListThreadSafe can be called on any sequence.
  observer_list_->AddObserver(observer);
}

void ScopedResourceUsageQuery::RemoveObserver(QueryResultObserver* observer) {
  // Must be called on the same sequence as AddObserver. ObserverListThreadSafe
  // will validate this.
  observer_list_->RemoveObserver(observer);
}

void ScopedResourceUsageQuery::Start(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unretained is safe because the destructor passes `params_` to the scheduler
  // sequence to delete.
  QueryScheduler::CallWithScheduler(base::BindOnce(
      &StartRepeatingQueryInScheduler, base::Unretained(params_.get())));
  throttled_timer_->StartTimer(delay, params_.get(), observer_list_);
}

void ScopedResourceUsageQuery::QueryOnce() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  throttled_timer_->SendRequestToScheduler(params_.get(), observer_list_,
                                           /*timer_fired=*/false);
}

QueryParams* ScopedResourceUsageQuery::GetParamsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return params_.get();
}

// static
base::TimeDelta ScopedResourceUsageQuery::GetMinMemoryQueryDelayForTesting() {
  return g_min_memory_query_delay;
}

ScopedResourceUsageQuery::ScopedResourceUsageQuery(
    base::PassKey<QueryBuilder>,
    std::unique_ptr<QueryParams> params)
    : params_(std::move(params)),
      throttled_timer_(std::make_unique<ThrottledTimer>()) {
  // Unretained is safe because the destructor passes `params_` to the scheduler
  // sequence to delete.
  QueryScheduler::CallWithScheduler(base::BindOnce(
      &AddScopedQueryToScheduler, base::Unretained(params_.get())));
}

// static
void ScopedResourceUsageQuery::NotifyObservers(
    scoped_refptr<ObserverList> observer_list,
    const QueryResultMap& results) {
  observer_list->Notify(FROM_HERE, &QueryResultObserver::OnResourceUsageUpdated,
                        results);
}

QueryBuilder::QueryBuilder() : params_(std::make_unique<QueryParams>()) {}

QueryBuilder::~QueryBuilder() = default;

QueryBuilder::QueryBuilder(QueryBuilder&&) = default;

QueryBuilder& QueryBuilder::operator=(QueryBuilder&&) = default;

QueryBuilder& QueryBuilder::AddResourceContext(const ResourceContext& context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  params_->contexts.AddResourceContext(context);
  return *this;
}

QueryBuilder& QueryBuilder::AddResourceType(ResourceType resource_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  params_->resource_types.Put(resource_type);
  return *this;
}

ScopedResourceUsageQuery QueryBuilder::CreateScopedQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidateQuery();
  // Pass ownership of `params_` to the scoped query, to avoid copying the
  // parameter contents.
  return ScopedResourceUsageQuery(base::PassKey<QueryBuilder>(),
                                  std::move(params_));
}

void QueryBuilder::QueryOnce(
    base::OnceCallback<void(const QueryResultMap&)> callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidateQuery();
  // Pass ownership of `params_` to the scheduler, to avoid copying the
  // parameter contents. QueryScheduler::RequestResult() will consume what it
  // needs from the params, which will then be deleted by the Owned() wrapper.
  QueryScheduler::CallWithScheduler(base::BindOnce(
      &RequestResultsFromScheduler, base::Owned(params_.release()),
      base::BindPostTask(task_runner, std::move(callback))));
}

QueryBuilder QueryBuilder::Clone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return QueryBuilder(params_->Clone());
}

QueryParams* QueryBuilder::GetParamsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return params_.get();
}

QueryBuilder::QueryBuilder(std::unique_ptr<QueryParams> params)
    : params_(std::move(params)) {}

QueryBuilder& QueryBuilder::AddAllContextsWithTypeId(
    internal::ResourceContextTypeId type_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  params_->contexts.AddAllContextsOfType(type_id);
  return *this;
}

void QueryBuilder::ValidateQuery() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  CHECK(!params_->contexts.IsEmpty());
  CHECK(!params_->resource_types.empty());
}

}  // namespace resource_attribution
