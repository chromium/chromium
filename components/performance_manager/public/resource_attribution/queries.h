// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERIES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERIES_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "base/types/variant_util.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/public/resource_attribution/type_helpers.h"

namespace performance_manager::resource_attribution {

namespace internal {
struct QueryParams;
}

class QueryBuilder;

// An observer that's notified by ScopedResourceUsageQuery whenever new results
// are available.
class QueryResultObserver {
 public:
  virtual ~QueryResultObserver() = default;
  virtual void OnResourceUsageUpdated(const QueryResultMap& results) = 0;
};

// Repeatedly makes resource attribution queries on a schedule as long as it's
// in scope.
// TODO(crbug.com/1471683): Unfinished. This registers on create and delete,
// which may have important side effects, but doesn't make scheduled queries
// yet. Use QueryOnce for now.
class ScopedResourceUsageQuery {
 public:
  ~ScopedResourceUsageQuery();

  // Move-only.
  ScopedResourceUsageQuery(ScopedResourceUsageQuery&&);
  ScopedResourceUsageQuery& operator=(ScopedResourceUsageQuery&&);
  ScopedResourceUsageQuery(const ScopedResourceUsageQuery&) = delete;
  ScopedResourceUsageQuery& operator=(const ScopedResourceUsageQuery&) = delete;

  // Adds an observer that will be notified on the calling sequence. Can be
  // called from any sequence.
  void AddObserver(QueryResultObserver* observer);

  // Removes an observer. Must be called from the same sequence as
  // AddObserver().
  void RemoveObserver(QueryResultObserver* observer);

  // Starts sending scheduled queries. They will repeat as long as the
  // ScopedResourceUsageQuery object exists. This must be called on the sequence
  // the object was created on.
  // TODO(crbug.com/1471683): Implement this.
  void Start();

  // Sends an immediate query, in addition to the schedule of repeated queries
  // triggered by Start(). This must be called on the sequence the
  // ScopedResourceUsageQuery object was created on.
  void QueryOnce();

  // Restricted implementation methods:

  // Gives tests access to validate the implementation.
  internal::QueryParams* GetParamsForTesting() const;

  // Private constructor for QueryBuilder. Use QueryBuilder::CreateScopedQuery()
  // to create a query.
  ScopedResourceUsageQuery(base::PassKey<QueryBuilder>,
                           std::unique_ptr<internal::QueryParams> params);

 private:
  using ObserverList = base::ObserverListThreadSafe<
      QueryResultObserver,
      base::RemoveObserverPolicy::kAddingSequenceOnly>;

  FRIEND_TEST_ALL_PREFIXES(ResourceAttrQueriesPMTest, ScopedQueryIsMovable);

  // Notifies `observer_list` that `results` were received.
  static void NotifyObservers(scoped_refptr<ObserverList> observer_list,
                              const QueryResultMap& results);

  SEQUENCE_CHECKER(sequence_checker_);

  // Parameters passed from the QueryBuilder.
  std::unique_ptr<internal::QueryParams> params_
      GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<ObserverList> observer_list_ =
      base::MakeRefCounted<ObserverList>();
};

// Creates a query to request resource usage measurements on a schedule.
//
// Use CreateScopedQuery() to return an object that makes repeated measurements
// as long as it's in scope, or QueryOnce() to take a single measurement. Before
// calling either of these, the query must specify:
//
//  * At least one resource type to measure, with AddResourceType().
//  * At least one resource context to attribute the measurements to, with
//    AddResourceContext() or AddAllContextsOfType().
//
// Example usage:
//
//   // To invoke `callback` with the CPU usage of all processes.
//   QueryBuilder()
//       .AddAllContextsOfType<ProcessContext>()
//       .AddResourceType(ResourceType::kCPUTime)
//       .QueryOnce(callback);
//
// QueryBuilder is move-only to prevent accidentally copying large state. Use
// Clone() to make an explicit copy.
class QueryBuilder {
 public:
  QueryBuilder();
  ~QueryBuilder();

  // Move-only.
  QueryBuilder(QueryBuilder&&);
  QueryBuilder& operator=(QueryBuilder&&);
  QueryBuilder(const QueryBuilder&) = delete;
  QueryBuilder& operator=(const QueryBuilder&) = delete;

  // Adds `context` to the list of resource contexts to query.
  QueryBuilder& AddResourceContext(const ResourceContext& context);

  // Adds all resource contexts of type ContextType to the list of resource
  // contexts to query. Whenever the query causes a resource measurement, all
  // resource contexts of the given type that exist at that moment will be
  // measured.
  template <typename ContextType,
            internal::EnableIfIsVariantAlternative<ContextType,
                                                   ResourceContext> = true>
  QueryBuilder& AddAllContextsOfType() {
    return AddAllContextsWithTypeIndex(
        base::VariantIndexOfType<ResourceContext, ContextType>());
  }

  // Add `type` to the lists of resources to query.
  QueryBuilder& AddResourceType(ResourceType resource_type);

  // Returns a scoped object that will repeatedly run the query and notify
  // observers with the results. Once this is called the QueryBuilder becomes
  // invalid.
  ScopedResourceUsageQuery CreateScopedQuery();

  // Runs the query and calls `callback` with the result. `callback` will be
  // invoked on `task_runner`. Once this is called the QueryBuilder becomes
  // invalid.
  // TODO(crbug.com/1471683): This takes an immediate measurement. Implement
  // more notification schedules.
  void QueryOnce(base::OnceCallback<void(const QueryResultMap&)> callback,
                 scoped_refptr<base::TaskRunner> task_runner =
                     base::SequencedTaskRunner::GetCurrentDefault());

  // Makes a copy of the QueryBuilder to use as a base for similar queries.
  QueryBuilder Clone() const;

  // Restricted implementation methods:

  // Gives tests access to validate the implementation.
  internal::QueryParams* GetParamsForTesting() const;

 private:
  // Private constructor for Clone().
  explicit QueryBuilder(std::unique_ptr<internal::QueryParams> params);

  // Implementation of AddAllContextsOfType().
  QueryBuilder& AddAllContextsWithTypeIndex(size_t index);

  // Asserts all members needed for QueryOnce() or CreateScopedQuery() are set.
  void ValidateQuery() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Parameters built up by the builder.
  std::unique_ptr<internal::QueryParams> params_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERIES_H_
