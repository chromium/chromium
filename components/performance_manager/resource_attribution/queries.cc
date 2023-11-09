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
  QueryScheduler::CallOnGraphWithScheduler(
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

internal::QueryParams* ScopedResourceUsageQuery::GetParamsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return params_.get();
}

ScopedResourceUsageQuery::ScopedResourceUsageQuery(
    base::PassKey<QueryBuilder>,
    std::unique_ptr<QueryParams> params)
    : params_(std::move(params)) {
  // Notify the scheduler this query exists.
  QueryScheduler::CallOnGraphWithScheduler(
      base::BindOnce(&AddScopedQueryToScheduler, params_.get()));
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
  // Pass ownership of `params_` to the scoped query, to avoid copying the
  // parameter contents.
  return ScopedResourceUsageQuery(base::PassKey<QueryBuilder>(),
                                  std::move(params_));
}

QueryBuilder QueryBuilder::Clone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clone the parameter contents to a newly-allocated QueryParams with the copy
  // constructor.
  auto cloned_params = std::make_unique<QueryParams>(*params_);
  return QueryBuilder(std::move(cloned_params));
}

internal::QueryParams* QueryBuilder::GetParamsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return params_.get();
}

QueryBuilder::QueryBuilder(std::unique_ptr<internal::QueryParams> params)
    : params_(std::move(params)) {}

QueryBuilder& QueryBuilder::AddAllContextsWithTypeIndex(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params_);
  params_->all_context_types.set(index);
  return *this;
}

}  // namespace performance_manager::resource_attribution
