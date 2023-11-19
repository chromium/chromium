// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/queries.h"

#include <bitset>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"

namespace performance_manager::resource_attribution {

namespace {

using QueryParams = internal::QueryParams;

void AddScopedQueryToScheduler(QueryParams* query_params,
                               QueryScheduler* scheduler) {
  scheduler->AddScopedQuery(query_params);
}

void RemoveScopedQueryFromScheduler(std::unique_ptr<QueryParams> query_params,
                                    QueryScheduler* scheduler) {
  scheduler->RemoveScopedQuery(std::move(query_params));
}

void RequestResultsFromScheduler(
    QueryParams* query_params,
    base::OnceCallback<void(const QueryResultMap&)> callback,
    QueryScheduler* scheduler) {
  scheduler->RequestResults(*query_params, std::move(callback));
}

}  // namespace

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

void ScopedResourceUsageQuery::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void ScopedResourceUsageQuery::QueryOnce() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unretained is safe because the destructor passes `params_` to the scheduler
  // sequence to delete.
  QueryScheduler::CallWithScheduler(base::BindOnce(
      &RequestResultsFromScheduler, base::Unretained(params_.get()),
      base::BindOnce(&ScopedResourceUsageQuery::NotifyObservers,
                     observer_list_)));
}

QueryParams* ScopedResourceUsageQuery::GetParamsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return params_.get();
}

ScopedResourceUsageQuery::ScopedResourceUsageQuery(
    base::PassKey<QueryBuilder>,
    std::unique_ptr<QueryParams> params)
    : params_(std::move(params)) {
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
  params_->resource_contexts.insert(context);
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
  // Clone the parameter contents to a newly-allocated QueryParams with the copy
  // constructor.
  auto cloned_params = std::make_unique<QueryParams>(*params_);
  return QueryBuilder(std::move(cloned_params));
}

QueryParams* QueryBuilder::GetParamsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return params_.get();
}

QueryBuilder::QueryBuilder(std::unique_ptr<QueryParams> params)
    : params_(std::move(params)) {}

QueryBuilder& QueryBuilder::AddAllContextsWithTypeIndex(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  params_->all_context_types.set(index);
  return *this;
}

void QueryBuilder::ValidateQuery() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  CHECK(!params_->resource_contexts.empty() ||
        !params_->all_context_types.none());
  CHECK(!params_->resource_types.Empty());
}

}  // namespace performance_manager::resource_attribution
