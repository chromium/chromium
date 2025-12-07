// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/browser_performance_scenarios.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/structured_shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

using performance_scenarios::PerformanceScenarioObserverList;
using performance_scenarios::ScenarioScope;

namespace {

void MaybeEmitNestingChangeEvent(
    const perfetto::NamedTrack* track,
    size_t old_nesting_level,
    size_t new_nesting_level,
    base::span<const perfetto::StaticString> event_names) {
  // Close trace events for each removed nesting level.
  for (size_t i = old_nesting_level; track && i > new_nesting_level; --i) {
    TRACE_EVENT_END("performance_scenarios", *track);
  }
  // Open trace events for each added nesting level.
  for (size_t i = old_nesting_level; track && i < new_nesting_level; ++i) {
    TRACE_EVENT_BEGIN("performance_scenarios", event_names.at(i), *track);
  }
}

// Generic methods that change according to the Scenario type.
template <typename Scenario>
struct ScenarioTraits {
  explicit ScenarioTraits(PerformanceScenarioData* state_ptr);

  // Returns a reference to the Scenario slot in shared memory.
  std::atomic<Scenario>& ScenarioRef();

  // Records trace events for a switch from `old_scenario` to `new_scenario` if
  // a tracing track is registered.
  void MaybeRecordTraceEvent(Scenario old_scenario,
                             Scenario new_scenario) const;

  // Notifies a ProcessNode's PerformanceScenarioObserver list of a switch from
  // `old_scenario` to `new_scenario`.
  void NotifyProcessObservers(Scenario old_scenario,
                              Scenario new_scenario) const;
};

template <>
struct ScenarioTraits<LoadingScenario> {
  explicit ScenarioTraits(PerformanceScenarioData* state_ptr)
      : state_ptr(state_ptr) {}

  std::atomic<LoadingScenario>& ScenarioRef() {
    return state_ptr->shared_state().WritableRef().loading;
  }

  void MaybeRecordTraceEvent(LoadingScenario old_scenario,
                             LoadingScenario new_scenario) const {
    MaybeEmitNestingChangeEvent(
        state_ptr->loading_tracing_track(), static_cast<size_t>(old_scenario),
        static_cast<size_t>(new_scenario),
        {"AnyPageLoading", "VisiblePageLoading", "FocusedPageLoading"});
  }

  void NotifyProcessObservers(LoadingScenario old_scenario,
                              LoadingScenario new_scenario) const {
    InputScenario input_scenario =
        state_ptr->shared_state().ReadOnlyRef().input.load(
            std::memory_order_relaxed);
    state_ptr->process_observer_list().NotifyScenariosChanged(
        old_scenario, new_scenario, input_scenario, input_scenario);
  }

  raw_ptr<PerformanceScenarioData> state_ptr;
};

template <>
struct ScenarioTraits<InputScenario> {
  explicit ScenarioTraits(PerformanceScenarioData* state_ptr)
      : state_ptr(state_ptr) {}

  std::atomic<InputScenario>& ScenarioRef() {
    return state_ptr->shared_state().WritableRef().input;
  }

  void MaybeRecordTraceEvent(InputScenario old_scenario,
                             InputScenario new_scenario) const {
    MaybeEmitNestingChangeEvent(state_ptr->input_tracing_track(),
                                static_cast<size_t>(old_scenario),
                                static_cast<size_t>(new_scenario),
                                {"TypingTapOrScroll", "TapOrScroll", "Scroll"});
  }

  void NotifyProcessObservers(InputScenario old_scenario,
                              InputScenario new_scenario) const {
    LoadingScenario loading_scenario =
        state_ptr->shared_state().ReadOnlyRef().loading.load(
            std::memory_order_relaxed);
    state_ptr->process_observer_list().NotifyScenariosChanged(
        loading_scenario, loading_scenario, old_scenario, new_scenario);
  }

  raw_ptr<PerformanceScenarioData> state_ptr;
};

// Holds the browser's global scenario state handle.
std::unique_ptr<PerformanceScenarioData>& GlobalSharedStatePtr() {
  static base::NoDestructor<std::unique_ptr<PerformanceScenarioData>> state_ptr;
  return *state_ptr;
}

// Returns a pointer to the shared memory region for communicating private state
// for `process_node`. Creates a region if none exists yet, returning nullptr on
// failure. The region's lifetime is tied to `process_node`.
PerformanceScenarioData* GetSharedStateForProcessNode(
    const ProcessNode* process_node) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto& data = PerformanceScenarioData::GetOrCreate(process_node);
  // Only return the process data if it holds a shared memory region.
  return data.HasSharedState() ? &data : nullptr;
}

// Returns a pointer to the global shared memory region that can be read by all
// processes, or nullptr if none exists. GlobalPerformanceScenarioMemory
// manages the lifetime of the region.
PerformanceScenarioData* GetGlobalSharedState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return GlobalSharedStatePtr().get();
}

// Sets the value for Scenario in the memory region held in `state_ptr` to
// `new_scenario`.
template <typename Scenario>
void SetScenarioValue(ScenarioScope scope,
                      Scenario new_scenario,
                      PerformanceScenarioData* state_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (state_ptr) {
    ScenarioTraits<Scenario> traits(state_ptr);
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    Scenario old_scenario =
        traits.ScenarioRef().exchange(new_scenario, std::memory_order_relaxed);
    if (old_scenario != new_scenario) {
      traits.MaybeRecordTraceEvent(old_scenario, new_scenario);
      switch (scope) {
        case ScenarioScope::kCurrentProcess:
          // Notify observers for the ProcessNode holding `state_ptr`.
          traits.NotifyProcessObservers(old_scenario, new_scenario);
          break;
        case ScenarioScope::kGlobal:
          // Notify all global observers registered in the browser process.
          if (auto observers = PerformanceScenarioObserverList::GetForScope(
                  ScenarioScope::kGlobal)) {
            observers->NotifyIfScenarioChanged();
          }
          break;
      }
    }
  }
}

template <typename Scenario>
void SetScenarioValueForRenderProcessHost(Scenario scenario,
                                          content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(host);
  CHECK(process_node);
  SetScenarioValue(ScenarioScope::kCurrentProcess, scenario,
                   GetSharedStateForProcessNode(process_node.get()));
}

}  // namespace

void SetGlobalSharedScenarioState(
    base::PassKey<ScopedGlobalScenarioMemory>,
    std::unique_ptr<PerformanceScenarioData> state) {
  // No BrowserThread::UI here because this might be called on the main thread
  // before browser threads are set up.
  CHECK_NE(state == nullptr, GlobalSharedStatePtr() == nullptr);
  GlobalSharedStatePtr() = std::move(state);
}

base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcessNode(
    const ProcessNode* process_node) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceScenarioData* state_ptr =
      GetSharedStateForProcessNode(process_node);
  // When this is called, the ProcessTrack should be available.
  if (state_ptr) {
    state_ptr->EnsureTracingTracks(process_node);
  }
  return state_ptr ? state_ptr->shared_state().DuplicateReadOnlyRegion()
                   : base::ReadOnlySharedMemoryRegion();
}

base::ReadOnlySharedMemoryRegion GetGlobalSharedScenarioRegion() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceScenarioData* state_ptr = GetGlobalSharedState();
  return state_ptr ? state_ptr->shared_state().DuplicateReadOnlyRegion()
                   : base::ReadOnlySharedMemoryRegion();
}

void SetLoadingScenarioForProcess(LoadingScenario scenario,
                                  content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValueForRenderProcessHost(scenario, host);
}

void SetLoadingScenarioForProcessNode(LoadingScenario scenario,
                                      const ProcessNode* process_node) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValue(ScenarioScope::kCurrentProcess, scenario,
                   GetSharedStateForProcessNode(process_node));
}

void SetGlobalLoadingScenario(LoadingScenario scenario) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValue(ScenarioScope::kGlobal, scenario, GetGlobalSharedState());
}

void SetInputScenarioForProcess(InputScenario scenario,
                                content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValueForRenderProcessHost(scenario, host);
}

void SetInputScenarioForProcessNode(InputScenario scenario,
                                    const ProcessNode* process_node) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValue(ScenarioScope::kCurrentProcess, scenario,
                   GetSharedStateForProcessNode(process_node));
}

void SetGlobalInputScenario(InputScenario scenario) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValue(ScenarioScope::kGlobal, scenario, GetGlobalSharedState());
}

}  // namespace performance_manager
