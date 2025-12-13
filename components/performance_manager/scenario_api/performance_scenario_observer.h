// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_OBSERVER_H_

#include "base/check.h"
#include "base/component_export.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/synchronization/lock.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory_forward.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_scenarios {

class MatchingScenarioObserver;
class PerformanceScenarioObserverList;

// An observer that watches for changes to values held in
// ScopedReadOnlyScenarioMemory.
class COMPONENT_EXPORT(SCENARIO_API) PerformanceScenarioObserver
    : public base::CheckedObserver {
 public:
  // Invoked whenever the given scenario changes for `scope`.
  virtual void OnLoadingScenarioChanged(ScenarioScope scope,
                                        LoadingScenario old_scenario,
                                        LoadingScenario new_scenario) {}
  virtual void OnInputScenarioChanged(ScenarioScope scope,
                                      InputScenario old_scenario,
                                      InputScenario new_scenario) {}
};

// An observer that watches for the scenario values held in
// ScopedReadOnlyScenarioMemory to start or stop matching a ScenarioPattern.
class COMPONENT_EXPORT(SCENARIO_API) MatchingScenarioObserver
    : public base::CheckedObserver {
 public:
  explicit MatchingScenarioObserver(ScenarioPattern pattern);

  ~MatchingScenarioObserver() override;

  MatchingScenarioObserver(const MatchingScenarioObserver&) = delete;
  MatchingScenarioObserver& operator=(const MatchingScenarioObserver&) = delete;

  // Returns the pattern that this observer matches.
  ScenarioPattern scenario_pattern() const { return pattern_; }

  // Invoked whenever the scenarios for `scope` change in a way that causes them
  // to start or stop matching the ScenarioPattern. `matches_pattern` will be
  // true if they now match the pattern, false otherwise.
  virtual void OnScenarioMatchChanged(ScenarioScope scope,
                                      bool matches_pattern) = 0;

 private:
  const ScenarioPattern pattern_;
};

// Central list of PerformanceScenarioObservers for a scope, wrapping an
// ObserverListThreadSafe. The lifetime is managed by
// ScopedScenarioObserverList on the main thread, but it's refcounted so
// GetForScope() can be called from any sequence. Callers on other sequences
// will extend the lifetime until they drop their reference.
//
// All methods can be called from any sequence.
class COMPONENT_EXPORT(SCENARIO_API) PerformanceScenarioObserverList
    : public base::RefCountedThreadSafe<PerformanceScenarioObserverList> {
 public:
  // Returns the object that notifies observers for `scope`, or nullptr if none
  // exists.
  static scoped_refptr<PerformanceScenarioObserverList> GetForScope(
      ScenarioScope scope);

  PerformanceScenarioObserverList(const PerformanceScenarioObserverList&) =
      delete;
  PerformanceScenarioObserverList& operator=(
      const PerformanceScenarioObserverList&) = delete;

  // Adds `observer` to the list. The observer will be notified on the calling
  // sequence.
  void AddObserver(PerformanceScenarioObserver* observer);

  // Removes `observer` from the list.
  void RemoveObserver(PerformanceScenarioObserver* observer);

  // Adds `matching_observer` to the list. The observer will be notified on the
  // calling sequence.
  void AddMatchingObserver(MatchingScenarioObserver* matching_observer);

  // Removes `matching_observer` from the list.
  void RemoveMatchingObserver(MatchingScenarioObserver* matching_observer);

  // Notifies observers of scenarios that have changed for this scope since the
  // last call.
  void NotifyIfScenarioChanged(
      const base::Location& location = base::Location::Current());

  // Notifies observers for all scopes of scenarios that have changed since the
  // last call.
  static void NotifyAllScopes(
      const base::Location& location = base::Location::Current());

  // Lets ScopedScenarioObserverList create and destroy the notifier for
  // `scope`.
  static void CreateForScope(base::PassKey<ScopedScenarioObserverList>,
                             ScenarioScope scope);
  static void DestroyForScope(base::PassKey<ScopedScenarioObserverList>,
                              ScenarioScope scope);

  // Lets ScopedReadOnlyScenarioMemory set the initial scenario state. After
  // this is called, observers will be notified when the state changes.
  void SetInitialScenarioState(
      base::PassKey<ScopedReadOnlyScenarioMemory>,
      scoped_refptr<RefCountedScenarioMapping> initial_mapping);

  bool IsInitializedForTesting();

 private:
  friend class base::RefCountedThreadSafe<PerformanceScenarioObserverList>;

  explicit PerformanceScenarioObserverList(ScenarioScope scope);
  ~PerformanceScenarioObserverList();

  const ScenarioScope scope_;

  base::Lock lock_;
  bool is_initialized_ GUARDED_BY(lock_) = false;

  // The last scenario values that were notified.
  LoadingScenario last_loading_scenario_ GUARDED_BY(lock_) =
      LoadingScenario::kNoPageLoading;
  InputScenario last_input_scenario_ GUARDED_BY(lock_) =
      InputScenario::kNoInput;

  // kAddingSequenceOnly is safer than the default so use it for all lists.
  template <typename T>
  using ObserverList = base::ObserverListThreadSafe<
      T,
      base::RemoveObserverPolicy::kAddingSequenceOnly>;

  // ObserverListThreadSafe must be held in a scoped_refptr because its
  // destructor is private, but the pointer should never be reassigned.
  const scoped_refptr<ObserverList<PerformanceScenarioObserver>> observers_ =
      base::MakeRefCounted<ObserverList<PerformanceScenarioObserver>>();

  // Lists of MatchingScenarioObservers that share the same ScenarioPattern,
  // along with the last `matches_pattern` value that was sent to
  // MatchingScenarioObserver::OnScenarioMatchChanged.
  struct MatchingScenarioObservers {
    bool last_matches_pattern = false;

    scoped_refptr<ObserverList<MatchingScenarioObserver>> observer_list =
        base::MakeRefCounted<ObserverList<MatchingScenarioObserver>>();

    MatchingScenarioObservers();
    ~MatchingScenarioObservers();

    // Move-only.
    MatchingScenarioObservers(const MatchingScenarioObservers&) = delete;
    MatchingScenarioObservers& operator=(const MatchingScenarioObservers&) =
        delete;
    MatchingScenarioObservers(MatchingScenarioObservers&&);
    MatchingScenarioObservers& operator=(MatchingScenarioObservers&&);
  };

  absl::flat_hash_map<ScenarioPattern, MatchingScenarioObservers>
      matching_observers_by_pattern_ GUARDED_BY(lock_);
};

}  // namespace performance_scenarios

namespace base {

// Specialize ScopedObservation to invoke the correct add and remove methods for
// MatchingScenarioObserver. These must be in the same namespace as
// base::ScopedObservationTraits.

template <>
struct ScopedObservationTraits<
    performance_scenarios::PerformanceScenarioObserverList,
    performance_scenarios::MatchingScenarioObserver> {
  static void AddObserver(
      performance_scenarios::PerformanceScenarioObserverList* source,
      performance_scenarios::MatchingScenarioObserver* observer) {
    source->AddMatchingObserver(observer);
  }
  static void RemoveObserver(
      performance_scenarios::PerformanceScenarioObserverList* source,
      performance_scenarios::MatchingScenarioObserver* observer) {
    source->RemoveMatchingObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_OBSERVER_H_
