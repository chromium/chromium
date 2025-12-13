// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenario_observer.h"

#include <atomic>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_scenarios {

namespace {

// The global pointers to PerformanceScenarioObserverLists are written from one
// thread, but read from several, so the pointers must be accessed under a lock.
// (As well as the pointed-to object having an atomic refcount.)
class LockedObserverListPtr {
 public:
  LockedObserverListPtr() = default;
  ~LockedObserverListPtr() = default;

  LockedObserverListPtr(const LockedObserverListPtr&) = delete;
  LockedObserverListPtr operator=(const LockedObserverListPtr&) = delete;

  // Returns a copy of the pointer.
  scoped_refptr<PerformanceScenarioObserverList> Get() {
    base::AutoLock lock(lock_);
    return observer_list_;
  }

  // Writes `observer_list` to the pointer, and returns the previous value.
  scoped_refptr<PerformanceScenarioObserverList> Exchange(
      scoped_refptr<PerformanceScenarioObserverList> observer_list) {
    base::AutoLock lock(lock_);
    return std::exchange(observer_list_, std::move(observer_list));
  }

 private:
  base::Lock lock_;
  scoped_refptr<PerformanceScenarioObserverList> observer_list_
      GUARDED_BY(lock_);
};

LockedObserverListPtr& GetLockedObserverListPtrForScope(ScenarioScope scope) {
  static base::NoDestructor<LockedObserverListPtr>
      current_process_observer_list;
  static base::NoDestructor<LockedObserverListPtr> global_observer_list;
  switch (scope) {
    case ScenarioScope::kCurrentProcess:
      return *current_process_observer_list;
    case ScenarioScope::kGlobal:
      return *global_observer_list;
  }
  NOTREACHED();
}

}  // namespace

MatchingScenarioObserver::MatchingScenarioObserver(ScenarioPattern pattern)
    : pattern_(pattern) {}

MatchingScenarioObserver::~MatchingScenarioObserver() = default;

// static
scoped_refptr<PerformanceScenarioObserverList>
PerformanceScenarioObserverList::GetForScope(ScenarioScope scope) {
  return GetLockedObserverListPtrForScope(scope).Get();
}

void PerformanceScenarioObserverList::AddObserver(
    PerformanceScenarioObserver* observer) {
  observers_->AddObserver(observer);
}

void PerformanceScenarioObserverList::RemoveObserver(
    PerformanceScenarioObserver* observer) {
  observers_->RemoveObserver(observer);
}

void PerformanceScenarioObserverList::AddMatchingObserver(
    MatchingScenarioObserver* matching_observer) {
  base::AutoLock lock(lock_);
  auto [it, inserted] = matching_observers_by_pattern_.try_emplace(
      matching_observer->scenario_pattern());
  if (inserted && is_initialized_) {
    // Initialize the scenario state for the new pattern.
    it->second.last_matches_pattern =
        CurrentScenariosMatch(scope_, matching_observer->scenario_pattern());
  }
  it->second.observer_list->AddObserver(matching_observer);
}

void PerformanceScenarioObserverList::RemoveMatchingObserver(
    MatchingScenarioObserver* matching_observer) {
  base::AutoLock lock(lock_);
  auto it = matching_observers_by_pattern_.find(
      matching_observer->scenario_pattern());
  CHECK(it != matching_observers_by_pattern_.end());
  if (it->second.observer_list->RemoveObserver(matching_observer) ==
      ObserverList<
          MatchingScenarioObserver>::RemoveObserverResult::kWasOrBecameEmpty) {
    matching_observers_by_pattern_.erase(it);
  }
}

void PerformanceScenarioObserverList::NotifyIfScenarioChanged(
    const base::Location& location) {
  base::AutoLock lock(lock_);
  if (!is_initialized_) {
    return;
  }
  LoadingScenario loading_scenario =
      GetLoadingScenario(scope_)->load(std::memory_order_relaxed);
  if (loading_scenario != last_loading_scenario_) {
    observers_->Notify(location,
                       &PerformanceScenarioObserver::OnLoadingScenarioChanged,
                       scope_, last_loading_scenario_, loading_scenario);
    last_loading_scenario_ = loading_scenario;
  }
  InputScenario input_scenario =
      GetInputScenario(scope_)->load(std::memory_order_relaxed);
  if (input_scenario != last_input_scenario_) {
    observers_->Notify(location,
                       &PerformanceScenarioObserver::OnInputScenarioChanged,
                       scope_, last_input_scenario_, input_scenario);
    last_input_scenario_ = input_scenario;
  }
  for (auto& [pattern, matching_observers] : matching_observers_by_pattern_) {
    bool matches_pattern =
        ScenariosMatch(loading_scenario, input_scenario, pattern);
    bool last_matches_pattern =
        std::exchange(matching_observers.last_matches_pattern, matches_pattern);
    if (last_matches_pattern != matches_pattern) {
      matching_observers.observer_list->Notify(
          location, &MatchingScenarioObserver::OnScenarioMatchChanged, scope_,
          matches_pattern);
    }
  }
}

// static
void PerformanceScenarioObserverList::NotifyAllScopes(
    const base::Location& location) {
  if (auto current_process_observers =
          GetForScope(ScenarioScope::kCurrentProcess)) {
    current_process_observers->NotifyIfScenarioChanged(location);
  }
  if (auto global_observers = GetForScope(ScenarioScope::kGlobal)) {
    global_observers->NotifyIfScenarioChanged(location);
  }
}

// static
void PerformanceScenarioObserverList::CreateForScope(
    base::PassKey<ScopedScenarioObserverList>,
    ScenarioScope scope) {
  auto old_ptr = GetLockedObserverListPtrForScope(scope).Exchange(
      base::WrapRefCounted(new PerformanceScenarioObserverList(scope)));
  CHECK(!old_ptr);
}

// static
void PerformanceScenarioObserverList::DestroyForScope(
    base::PassKey<ScopedScenarioObserverList>,
    ScenarioScope scope) {
  // Drop the main owning reference. Callers of GetForScope() might still have
  // references, but no new caller can obtain a reference.
  auto old_ptr = GetLockedObserverListPtrForScope(scope).Exchange(nullptr);
  CHECK(old_ptr);
}

void PerformanceScenarioObserverList::SetInitialScenarioState(
    base::PassKey<ScopedReadOnlyScenarioMemory>,
    scoped_refptr<RefCountedScenarioMapping> initial_mapping) {
  base::AutoLock lock(lock_);
  CHECK(!is_initialized_);
  // Read the scenario state directly from `initial_mapping`, because the public
  // GetLoadingScenario() and GetInputScenario() accessors will deadlock here.
  // This is called with a lock held while initializing the global
  // RefCountedScenarioMapping pointer, and the public accessors take the same
  // lock to read the pointer.
  last_loading_scenario_ = initial_mapping->data.ReadOnlyRef().loading.load(
      std::memory_order_relaxed);
  last_input_scenario_ =
      initial_mapping->data.ReadOnlyRef().input.load(std::memory_order_relaxed);
  // Initialize any MatchingObservers that were already added.
  for (auto& [pattern, matching_observers] : matching_observers_by_pattern_) {
    matching_observers.last_matches_pattern =
        ScenariosMatch(last_loading_scenario_, last_input_scenario_, pattern);
  }
  is_initialized_ = true;
}

bool PerformanceScenarioObserverList::IsInitializedForTesting() {
  base::AutoLock lock(lock_);
  return is_initialized_;
}

PerformanceScenarioObserverList::PerformanceScenarioObserverList(
    ScenarioScope scope)
    : scope_(scope) {}

PerformanceScenarioObserverList::~PerformanceScenarioObserverList() = default;

PerformanceScenarioObserverList::MatchingScenarioObservers::
    MatchingScenarioObservers() = default;

PerformanceScenarioObserverList::MatchingScenarioObservers::
    ~MatchingScenarioObservers() = default;

PerformanceScenarioObserverList::MatchingScenarioObservers::
    MatchingScenarioObservers(MatchingScenarioObservers&&) = default;

PerformanceScenarioObserverList::MatchingScenarioObservers&
PerformanceScenarioObserverList::MatchingScenarioObservers::operator=(
    MatchingScenarioObservers&&) = default;

}  // namespace performance_scenarios
