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
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/public/resource_attribution/type_helpers.h"

namespace resource_attribution {

namespace internal {
class QueryParams;
class QueryScheduler;
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
  // TODO(crbug.com/40926264): Repeating queries will be sent on a timer with
  // `delay` between queries. Replace this with the full scheduling hints
  // described at https://bit.ly/resource-attribution-api#heading=h.upcqivkhbs4t
  void Start(base::TimeDelta delay);

  // Sends an immediate query, in addition to the schedule of repeated queries
  // triggered by Start(). This must be called on the sequence the
  // ScopedResourceUsageQuery object was created on.
  void QueryOnce();

  // Restricted implementation methods:

  // Gives tests access to validate the implementation.
  internal::QueryParams* GetParamsForTesting() const;

  // Returns the minimum delay between QueryOnce() calls for kMemorySummary
  // resources.
  static base::TimeDelta GetMinMemoryQueryDelayForTesting();

  // Instantiate this to set the minimum delay between QueryOnce() calls for
  // kMemorySummary resources to 0 during a test.
  class ScopedDisableMemoryQueryDelayForTesting {
   public:
    ScopedDisableMemoryQueryDelayForTesting();
    ~ScopedDisableMemoryQueryDelayForTesting();

   private:
    base::TimeDelta previous_delay_;
  };

  // Private constructor for QueryBuilder. Use QueryBuilder::CreateScopedQuery()
  // to create a query.
  ScopedResourceUsageQuery(base::PassKey<QueryBuilder>,
                           std::unique_ptr<internal::QueryParams> params);

 private:
  using ObserverList = base::ObserverListThreadSafe<
      QueryResultObserver,
      base::RemoveObserverPolicy::kAddingSequenceOnly>;

  FRIEND_TEST_ALL_PREFIXES(ResourceAttrQueriesPMTest, ScopedQueryIsMovable);

  class ThrottledTimer;

  // Notifies `observer_list` that `results` were received.
  static void NotifyObservers(scoped_refptr<ObserverList> observer_list,
                              const QueryResultMap& results);

  SEQUENCE_CHECKER(sequence_checker_);

  // Parameters passed from the QueryBuilder.
  std::unique_ptr<internal::QueryParams> params_
      GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<ObserverList> observer_list_ =
      base::MakeRefCounted<ObserverList>();

  // A base::RepeatingTimer used to schedule repeating queries, and some
  // tracking data to throttle QueryOnce() calls so they don't interfere. This
  // is in a pointer because ScopedResourceUsageQuery is movable but
  // RepeatingTimer isn't.
  // TODO(crbug.com/40926264): Manage timing centrally in QueryScheduler.
  std::unique_ptr<ThrottledTimer> throttled_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// Convenience alias for a ScopedObservation that observes a
// ScopedResourceUsageQuery.
using ScopedQueryObservation =
    base::ScopedObservation<ScopedResourceUsageQuery, QueryResultObserver>;

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
    return AddAllContextsWithTypeId(
        internal::ResourceContextTypeId::ForType<ContextType>());
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
  // TODO(crbug.com/40926264): This takes an immediate measurement. Implement
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
  QueryBuilder& AddAllContextsWithTypeId(
      internal::ResourceContextTypeId type_id);

  // Asserts all members needed for QueryOnce() or CreateScopedQuery() are set.
  void ValidateQuery() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Parameters built up by the builder.
  std::unique_ptr<internal::QueryParams> params_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERIES_H_
