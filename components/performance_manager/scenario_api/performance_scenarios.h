// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIOS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIOS_H_

#include <atomic>
#include <utility>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory_forward.h"

namespace performance_scenarios {

// Defines performance scenarios that a page can be in.
//
// Each enum is a list of mutually-exclusive scenarios. The complete scenario
// state is a tuple of all scenarios that are detected, at most one from each
// enum.
//
// The browser process detects which scenarios apply and shares that state with
// child processes over shared memory. Each process can view a global scenario
// list over the entire browser (eg. some page is loading) or a scenario list
// targeted only to that process (eg. a page hosted in this process is loading).
//
// Additional functions to let the browser process query the performance
// scenarios for a child process are in
// components/performance_manager/public/scenarios/process_performance_scenarios.h.

// Scenarios indicating a page is loading, ordered from least-specific to
// most-specific.
enum class LoadingScenario {
  // No pages covered by the scenario are loading.
  kNoPageLoading = 0,
  // No visible pages are loading, but a non-visible page is.
  kBackgroundPageLoading,
  // The focused page (if any) is not loading, but a visible page is.
  kVisiblePageLoading,
  // The focused page is loading. Implies the page is also visible.
  kFocusedPageLoading,
  kMax = kFocusedPageLoading,
};
using LoadingScenarios = base::EnumSet<LoadingScenario,
                                       LoadingScenario::kNoPageLoading,
                                       LoadingScenario::kMax>;

// Scenarios indicating user input, ordered from least-specific to
// most-specific.
enum class InputScenario {
  // No input was detected.
  kNoInput = 0,
  // The user is typing in a focused page. There were no recent taps or scrolls.
  kTyping,
  // The user tapped a focused page. There may be recent typing, but not
  // scrolls.
  kTap,
  // The user is scrolling in a focused page. There may also be recent typing or
  // taps.
  kScroll,
  kMax = kScroll,
};
using InputScenarios =
    base::EnumSet<InputScenario, InputScenario::kNoInput, InputScenario::kMax>;

// The scope that a scenario covers.
enum class ScenarioScope {
  // The scenario covers only pages hosted in the current process.
  kCurrentProcess,
  // The scenario covers the whole browser.
  kGlobal,
};
using ScenarioScopes = base::EnumSet<ScenarioScope,
                                     /*Min=*/ScenarioScope::kCurrentProcess,
                                     /*Max=*/ScenarioScope::kGlobal>;

// Different subsets of scenarios that can be checked with the ScenariosMatch()
// function or a MatchingScenarioObserver.
//
// A given ScenarioScope `scope` matches a ScenarioPattern if all of:
//
// * GetLoadingScenario(scope) returns a value in the `loading` set, or the set
//   is empty.
// * GetInputScenarios(scope) returns a value in the `input` set, or the set is
//   empty.
struct COMPONENT_EXPORT(SCENARIO_API) ScenarioPattern {
  // Set of LoadingScenarios that match the pattern. If this is empty, any
  // LoadingScenario matches.
  LoadingScenarios loading;

  // Set of InputScenarios that match the pattern. If this is empty, any
  // InputScenario matches.
  InputScenarios input;
};

// A ScenarioPattern for a scope that's considered "idle": only background pages
// are loading and there is no input. This is a good definition of "idle" for
// most purposes, but some features that are particularly sensitive to different
// scenarios may want to define a different ScenarioPattern.
inline constexpr ScenarioPattern kDefaultIdleScenarios{
    .loading = {LoadingScenario::kNoPageLoading,
                LoadingScenario::kBackgroundPageLoading},
    .input = {InputScenario::kNoInput},
};

// A wrapper around a std::atomic<T> that's stored in shared memory. The wrapper
// prevents the shared memory from being unmapped while a caller has a reference
// to the atomic. Dereference the SharedAtomicRef to read from it as a
// std::atomic. See the comments above GetLoadingScenario() for usage notes.
template <typename T>
class SharedAtomicRef {
 public:
  SharedAtomicRef(scoped_refptr<RefCountedScenarioMapping> mapping,
                  const std::atomic<T>& wrapped_atomic)
      : mapping_(std::move(mapping)), wrapped_atomic_(wrapped_atomic) {}

  ~SharedAtomicRef() = default;

  // Move-only.
  SharedAtomicRef(const SharedAtomicRef&) = delete;
  SharedAtomicRef& operator=(const SharedAtomicRef&) = delete;
  SharedAtomicRef(SharedAtomicRef&&) = default;
  SharedAtomicRef& operator=(SharedAtomicRef&&) = default;

  // Smart-pointer-like interface:

  // Returns a pointer to the wrapped atomic.
  const std::atomic<T>* get() const { return &wrapped_atomic_; }

  // Returns a reference to the wrapped atomic.
  const std::atomic<T>& operator*() const { return wrapped_atomic_; }

  // Returns a pointer to the wrapped atomic for method invocation.
  const std::atomic<T>* operator->() const { return &wrapped_atomic_; }

 private:
  const scoped_refptr<RefCountedScenarioMapping> mapping_;

  // A reference into `mapping_`, not PartitionAlloc memory.
  RAW_PTR_EXCLUSION const std::atomic<T>& wrapped_atomic_;
};

// Functions to query performance scenarios.
//
// Since the scenarios can be modified at any time from another process, they're
// accessed through SharedAtomicRef. Get a snapshot of the scenario with
// std::atomic::load(). std::memory_order_relaxed is usually sufficient since no
// other memory depends on the scenario value.
//
// Usage:
//
//   // Test whether any foreground page is loading.
//   LoadingScenario scenario = GetLoadingScenario(ScenarioScope::kGlobal)
//                                 ->load(std::memory_order_relaxed);
//   if (scenario == LoadingScenario::kFocusedPageLoading ||
//       scenario == LoadingScenario::kVisiblePageLoading) {
//     ... delay less-important work until scenario changes ...
//   }
//
//   // Inverse of the above test: true if NO foreground page is loading.
//   if (CurrentScenariosMatch(ScenarioScope::kGlobal,
//                             ScenarioPattern{.loading = {
//                               LoadingScenario::kNoPageLoading,
//                               LoadingScenario::kBackgroundPageLoading,
//                             }) {
//     ... good time to do less-important work ...
//   }
//
//   // Test whether the current process is in the critical path for user input.
//   if (GetInputScenario(ScenarioScope::kCurrentProcess)->load(
//           std::memory_order_relaxed) != InputScenario::kNoInput) {
//     ... current process should prioritize input responsiveness ...
//   }
//
//   // Equivalently:
//   if (!CurrentScenariosMatch(ScenarioScope::kCurrentProcess,
//                              ScenarioPattern{
//                                .input = {InputScenario::kNoInput}
//                              }) {
//     ... current process should prioritize input responsiveness ...
//   }
//
//   // Test whether the browser overall is idle by the most common definition.
//   if (CurrentScenariosMatch(ScenarioScope::kGlobal, kDefaultIdleScenarios)) {
//     ... good time to do maintenance tasks ...
//   }

// Returns a reference to the loading scenario for `scope`.
COMPONENT_EXPORT(SCENARIO_API)
SharedAtomicRef<LoadingScenario> GetLoadingScenario(ScenarioScope scope);

// Returns a reference to the input scenario for `scope`.
COMPONENT_EXPORT(SCENARIO_API)
SharedAtomicRef<InputScenario> GetInputScenario(ScenarioScope scope);

// Returns true if `scope` currently matches `pattern`.
COMPONENT_EXPORT(SCENARIO_API)
bool CurrentScenariosMatch(ScenarioScope scope, ScenarioPattern pattern);

// Returns true if the given scenarios match `pattern`.
COMPONENT_EXPORT(SCENARIO_API)
bool ScenariosMatch(LoadingScenario loading_scenario,
                    InputScenario input_scenario,
                    ScenarioPattern pattern);

}  // namespace performance_scenarios

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIOS_H_
