// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_OBSERVER_H_

#include <array>

#include "base/component_export.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory_forward.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_scenarios {

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

  // Allows PerformanceScenarioObserverList to notify of scenario changes. Will
  // invoke OnScenarioMatchChanged if necessary.
  void NotifyIfScenarioMatchChanged(ScenarioScope scope,
                                    LoadingScenario loading_scenario,
                                    InputScenario input_scenario);

 private:
  // Returns a reference into `last_match_notifications_` for `scope`.
  bool& LastMatchNotification(ScenarioScope scope);

  const ScenarioPattern pattern_;

  // This will be bound to whichever sequence calls
  // PerformanceScenarioObserverList::AddMatchingObserver, to validate that the
  // observer list always accesses this class on that sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // The last match notification that was sent for each scope.
  std::array<bool, ScenarioScopes::kValueCount> last_match_notifications_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// Central list of PerformanceScenarioObservers for a scope, wrapping an
// ObserverListThreadSafe. The lifetime is managed by
// ScopedReadOnlyScenarioMemory on the main thread, but it's refcounted so
// GetForScope() can be called from any sequence. Callers on other sequences
// will extend the lifetime until they drop their reference.
//
// All methods can be called from any sequence.
class COMPONENT_EXPORT(SCENARIO_API) PerformanceScenarioObserverList
    : public base::RefCountedThreadSafe<PerformanceScenarioObserverList> {
 public:
  // Returns the object that notifies observers for `scope`, or nullptr if no
  // ScopedReadOnlyScenarioMemory exists for `scope`.
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
      base::Location location = base::Location::Current());

  // Notifies observers for all scopes of scenarios that have changed since the
  // last call.
  static void NotifyAllScopes(
      base::Location location = base::Location::Current());

  // Lets ScopedReadOnlyScenarioMemory create and destroy the notifier for
  // `scope`.
  static void CreateForScope(base::PassKey<ScopedReadOnlyScenarioMemory>,
                             ScenarioScope scope);
  static void DestroyForScope(base::PassKey<ScopedReadOnlyScenarioMemory>,
                              ScenarioScope scope);

 private:
  friend class base::RefCountedThreadSafe<PerformanceScenarioObserverList>;

  explicit PerformanceScenarioObserverList(ScenarioScope scope);
  ~PerformanceScenarioObserverList();

  const ScenarioScope scope_;

  // The last scenario values that were notified.
  base::Lock lock_;
  LoadingScenario last_loading_scenario_ GUARDED_BY(lock_);
  InputScenario last_input_scenario_ GUARDED_BY(lock_);

  // kAddingSequenceOnly is safer than the default so use it for all lists.
  template <typename T>
  using ObserverList = base::ObserverListThreadSafe<
      T,
      base::RemoveObserverPolicy::kAddingSequenceOnly>;

  // ObserverListThreadSafe must be held in a scoped_refptr because its
  // destructor is private, but the pointer should never be reassigned.
  const scoped_refptr<ObserverList<PerformanceScenarioObserver>> observers_ =
      base::MakeRefCounted<ObserverList<PerformanceScenarioObserver>>();
  const scoped_refptr<ObserverList<MatchingScenarioObserver>>
      matching_observers_ =
          base::MakeRefCounted<ObserverList<MatchingScenarioObserver>>();
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
