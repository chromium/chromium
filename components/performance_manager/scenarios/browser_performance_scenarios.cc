// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/browser_performance_scenarios.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/structured_shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
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
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

using performance_scenarios::ScenarioScope;

namespace {

// Generic methods that change according to the Scenario type.
template <typename Scenario>
struct ScenarioTraits {
  explicit ScenarioTraits(PerformanceScenarioData* state_ptr);

  // Returns a reference to the Scenario slot in shared memory.
  std::atomic<Scenario>& ScenarioRef();

  // Opens a trace event for `scenario` if a tracing track is registered.
  void MaybeBeginTraceEvent(Scenario scenario) const;

  // Closes the trace event for `scenario` if a tracing track is registered.
  void MaybeEndTraceEvent(Scenario scenario) const;
};

template <>
struct ScenarioTraits<LoadingScenario> {
  explicit ScenarioTraits(PerformanceScenarioData* state_ptr)
      : state_ptr(state_ptr) {}

  std::atomic<LoadingScenario>& ScenarioRef() {
    return state_ptr->shared_state().WritableRef().loading;
  }

  void MaybeBeginTraceEvent(LoadingScenario scenario) const {
    if (!state_ptr->loading_tracing_track()) {
      return;
    }
    switch (scenario) {
      case LoadingScenario::kNoPageLoading:
        // No trace event.
        return;
      case LoadingScenario::kBackgroundPageLoading:
        TRACE_EVENT_BEGIN("loading", "BackgroundPageLoading",
                          *state_ptr->loading_tracing_track());
        return;
      case LoadingScenario::kVisiblePageLoading:
        TRACE_EVENT_BEGIN("loading", "VisiblePageLoading",
                          *state_ptr->loading_tracing_track());
        return;
      case LoadingScenario::kFocusedPageLoading:
        TRACE_EVENT_BEGIN("loading", "FocusedPageLoading",
                          *state_ptr->loading_tracing_track());
        return;
    }
    NOTREACHED();
  }

  void MaybeEndTraceEvent(LoadingScenario scenario) const {
    if (!state_ptr->loading_tracing_track()) {
      return;
    }
    switch (scenario) {
      case LoadingScenario::kNoPageLoading:
        // No trace event.
        return;
      case LoadingScenario::kBackgroundPageLoading:
      case LoadingScenario::kVisiblePageLoading:
      case LoadingScenario::kFocusedPageLoading:
        TRACE_EVENT_END("loading", *state_ptr->loading_tracing_track());
        return;
    }
    NOTREACHED();
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

  void MaybeBeginTraceEvent(InputScenario scenario) const {
    if (!state_ptr->input_tracing_track()) {
      return;
    }
    switch (scenario) {
      case InputScenario::kNoInput:
        // No trace event.
        return;
      case InputScenario::kTyping:
        TRACE_EVENT_BEGIN("input", "Typing", *state_ptr->input_tracing_track());
        return;
    }
    NOTREACHED();
  }

  void MaybeEndTraceEvent(InputScenario scenario) const {
    if (!state_ptr->input_tracing_track()) {
      return;
    }
    switch (scenario) {
      case InputScenario::kNoInput:
        // No trace event.
        return;
      case InputScenario::kTyping:
        TRACE_EVENT_END("input", *state_ptr->input_tracing_track());
        return;
    }
    NOTREACHED();
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
void SetScenarioValue(Scenario new_scenario,
                      PerformanceScenarioData* state_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (state_ptr) {
    ScenarioTraits<Scenario> traits(state_ptr);
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    Scenario old_scenario =
        traits.ScenarioRef().exchange(new_scenario, std::memory_order_relaxed);
    if (old_scenario != new_scenario) {
      traits.MaybeEndTraceEvent(old_scenario);
      traits.MaybeBeginTraceEvent(new_scenario);
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
  SetScenarioValue(scenario, GetSharedStateForProcessNode(process_node.get()));
}

template <typename Scenario>
void SetGlobalScenarioValue(Scenario scenario) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValue(scenario, GetGlobalSharedState());
  // Notify kGlobal observers in the browser process.
  if (auto observers =
          performance_scenarios::PerformanceScenarioObserverList::GetForScope(
              ScenarioScope::kGlobal)) {
    observers->NotifyIfScenarioChanged();
  }
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
  SetScenarioValue(scenario, GetSharedStateForProcessNode(process_node));
}

void SetGlobalLoadingScenario(LoadingScenario scenario) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetGlobalScenarioValue(scenario);
}

void SetInputScenarioForProcess(InputScenario scenario,
                                content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValueForRenderProcessHost(scenario, host);
}

void SetInputScenarioForProcessNode(InputScenario scenario,
                                    const ProcessNode* process_node) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetScenarioValue(scenario, GetSharedStateForProcessNode(process_node));
}

void SetGlobalInputScenario(InputScenario scenario) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetGlobalScenarioValue(scenario);
}

}  // namespace performance_manager
