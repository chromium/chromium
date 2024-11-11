// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/trace_event/typed_macros.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/scenarios/performance_scenario_observer.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

// Shim to get observer lists from PerformanceScenarioNotifier.
class PerformanceScenarioNotifierAccessor {
 public:
  static base::ObserverList<PerformanceScenarioObserver>* GetGlobalObservers(
      Graph* graph) {
    if (auto* notifier = PerformanceScenarioNotifier::GetFromGraph(graph)) {
      return notifier->GetGlobalObservers();
    }
    return nullptr;
  }

  static base::ObserverList<PerformanceScenarioObserver>* GetProcessObservers(
      const ProcessNode* process_node) {
    if (auto* notifier = PerformanceScenarioNotifier::GetFromGraph(
            process_node->GetGraph())) {
      return notifier->GetProcessObservers(process_node);
    }
    return nullptr;
  }
};

namespace {

// Generic methods that change according to the Scenario type.
template <typename Scenario>
struct ScenarioTraits {
  explicit ScenarioTraits(scoped_refptr<RefCountedScenarioState> state_ptr);

  // Returns a reference to the Scenario slot in shared memory.
  std::atomic<Scenario>& ScenarioRef();

  // Opens a trace event for `scenario` if a tracing track is registered.
  void MaybeBeginTraceEvent(Scenario scenario) const;

  // Closes the trace event for `scenario` if a tracing track is registered.
  void MaybeEndTraceEvent(Scenario scenario) const;

  // ProcessObserver methods called by ObserverList::Notify() for this scenario.
  static constexpr void (PerformanceScenarioObserver::*kGlobalNotifyMethod)(
      Scenario,
      Scenario) = nullptr;
  static constexpr void (PerformanceScenarioObserver::*kProcessNotifyMethod)(
      const ProcessNode*,
      Scenario,
      Scenario) = nullptr;
};

template <>
struct ScenarioTraits<LoadingScenario> {
  explicit ScenarioTraits(scoped_refptr<RefCountedScenarioState> state_ptr)
      : state_ptr(std::move(state_ptr)) {}

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

  static constexpr void (PerformanceScenarioObserver::*kGlobalNotifyMethod)(
      LoadingScenario,
      LoadingScenario) =
      &PerformanceScenarioObserver::OnGlobalLoadingScenarioChanged;

  static constexpr void (PerformanceScenarioObserver::*kProcessNotifyMethod)(
      const ProcessNode*,
      LoadingScenario,
      LoadingScenario) =
      &PerformanceScenarioObserver::OnProcessLoadingScenarioChanged;

  scoped_refptr<RefCountedScenarioState> state_ptr;
};

template <>
struct ScenarioTraits<InputScenario> {
  explicit ScenarioTraits(scoped_refptr<RefCountedScenarioState> state_ptr)
      : state_ptr(std::move(state_ptr)) {}

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
    }
    NOTREACHED();
  }

  static constexpr void (PerformanceScenarioObserver::*kGlobalNotifyMethod)(
      InputScenario,
      InputScenario) =
      &PerformanceScenarioObserver::OnGlobalInputScenarioChanged;
  static constexpr void (PerformanceScenarioObserver::*kProcessNotifyMethod)(
      const ProcessNode*,
      InputScenario,
      InputScenario) =
      &PerformanceScenarioObserver::OnProcessInputScenarioChanged;

  scoped_refptr<RefCountedScenarioState> state_ptr;
};

// Holds the browser's global scenario state handle.
scoped_refptr<RefCountedScenarioState>& GlobalSharedStatePtr() {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioState>> state_ptr;
  return *state_ptr;
}

// Returns a pointer to the shared memory region for communicating private state
// for `process_node`. Creates a region if none exists yet, returning nullptr on
// failure. The region's lifetime is tied to `process_node`. Must be called from
// the PM sequence.
scoped_refptr<RefCountedScenarioState> GetSharedStateForProcessNode(
    const ProcessNode* process_node) {
  // Returns a copy of the pointer.
  return PerformanceScenarioMemoryData::GetOrCreate(process_node).state_ptr;
}

// Returns a pointer to the global shared memory region that can be read by all
// processes, or nullptr if none exists. GlobalPerformanceScenarioMemory
// manages the lifetime of the region.
scoped_refptr<RefCountedScenarioState> GetGlobalSharedState() {
  // Returns a copy of the pointer.
  return GlobalSharedStatePtr();
}

// Sets the value for Scenario in the memory region held in `state_ptr` to
// `new_scenario`, and returns the old value.
template <typename Scenario>
Scenario SetScenarioValue(Scenario new_scenario,
                          scoped_refptr<RefCountedScenarioState> state_ptr) {
  if (state_ptr) {
    ScenarioTraits<Scenario> traits(std::move(state_ptr));
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    Scenario old_scenario =
        traits.ScenarioRef().exchange(new_scenario, std::memory_order_relaxed);
    if (old_scenario != new_scenario) {
      traits.MaybeEndTraceEvent(old_scenario);
      traits.MaybeBeginTraceEvent(new_scenario);
    }
    return old_scenario;
  }
  // Pretend the scenario already had this value, to not trigger observers.
  return new_scenario;
}

template <typename Scenario>
void SetScenarioValueForProcessNode(Scenario scenario,
                                    const ProcessNode* process_node) {
  Scenario old_scenario =
      SetScenarioValue(scenario, GetSharedStateForProcessNode(process_node));
  if (old_scenario != scenario) {
    auto* observers =
        PerformanceScenarioNotifierAccessor::GetProcessObservers(process_node);
    if (observers) {
      observers->Notify(ScenarioTraits<Scenario>::kProcessNotifyMethod,
                        process_node, old_scenario, scenario);
    }
  }
}

template <typename Scenario>
void SetScenarioValueForRenderProcessHost(Scenario scenario,
                                          content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](Scenario scenario, base::WeakPtr<ProcessNode> process_node) {
            if (process_node) {
              SetScenarioValueForProcessNode(scenario, process_node.get());
            }
          },
          scenario,
          PerformanceManager::GetProcessNodeForRenderProcessHost(host)));
}

template <typename Scenario>
void SetGlobalScenarioValue(Scenario scenario) {
  Scenario old_scenario = SetScenarioValue(scenario, GetGlobalSharedState());
  if (old_scenario == scenario) {
    return;
  }
  // The global scenario can be set on any thread, but the observers must be
  // notified on the PM sequence.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](Scenario scenario, Scenario old_scenario, Graph* graph) {
            auto* observers =
                PerformanceScenarioNotifierAccessor::GetGlobalObservers(graph);
            if (observers) {
              observers->Notify(ScenarioTraits<Scenario>::kGlobalNotifyMethod,
                                old_scenario, scenario);
            }
          },
          scenario, old_scenario));
}

}  // namespace

ScopedGlobalScenarioMemory::ScopedGlobalScenarioMemory() {
  CHECK(!GlobalSharedStatePtr());
  auto state_ptr = RefCountedScenarioState::Create();
  if (state_ptr) {
    state_ptr->EnsureTracingTracks();
    GlobalSharedStatePtr() = std::move(state_ptr);
    read_only_mapping_.emplace(blink::performance_scenarios::Scope::kGlobal,
                               GetGlobalSharedScenarioRegion());
  }
}

ScopedGlobalScenarioMemory::~ScopedGlobalScenarioMemory() {
  GlobalSharedStatePtr().reset();
}

base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcessNode(
    const ProcessNode* process_node) {
  auto state_ptr = GetSharedStateForProcessNode(process_node);
  // When this is called, the ProcessTrack should be available.
  if (state_ptr) {
    state_ptr->EnsureTracingTracks(process_node);
  }
  return state_ptr ? state_ptr->shared_state().DuplicateReadOnlyRegion()
                   : base::ReadOnlySharedMemoryRegion();
}

base::ReadOnlySharedMemoryRegion GetGlobalSharedScenarioRegion() {
  auto state_ptr = GetGlobalSharedState();
  return state_ptr ? state_ptr->shared_state().DuplicateReadOnlyRegion()
                   : base::ReadOnlySharedMemoryRegion();
}

void SetLoadingScenarioForProcess(LoadingScenario scenario,
                                  content::RenderProcessHost* host) {
  SetScenarioValueForRenderProcessHost(scenario, host);
}

void SetLoadingScenarioForProcessNode(LoadingScenario scenario,
                                      const ProcessNode* process_node) {
  SetScenarioValueForProcessNode(scenario, process_node);
}

void SetGlobalLoadingScenario(LoadingScenario scenario) {
  SetGlobalScenarioValue(scenario);
}

void SetInputScenarioForProcess(InputScenario scenario,
                                content::RenderProcessHost* host) {
  SetScenarioValueForRenderProcessHost(scenario, host);
}

void SetInputScenarioForProcessNode(InputScenario scenario,
                                    const ProcessNode* process_node) {
  SetScenarioValueForProcessNode(scenario, process_node);
}

void SetGlobalInputScenario(InputScenario scenario) {
  SetGlobalScenarioValue(scenario);
}

}  // namespace performance_manager
