// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

namespace {

using SharedScenarioState = blink::performance_scenarios::SharedScenarioState;

// Holds the browser's global scenario state handle.
scoped_refptr<RefCountedScenarioState>& GlobalSharedStatePtr() {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioState>> state_ptr;
  return *state_ptr;
}

PerformanceScenarioMemoryData& GetOrCreatePerformanceScenarioMemoryData(
    ProcessNodeImpl* process_node_impl) {
  if (PerformanceScenarioMemoryData::Exists(process_node_impl)) {
    return PerformanceScenarioMemoryData::Get(process_node_impl);
  }
  return PerformanceScenarioMemoryData::Create(process_node_impl);
}

// Returns a pointer to the shared memory region for communicating private state
// for `process_node`. Creates a region if none exists yet, returning nullptr on
// failure. The region's lifetime is tied to `process_node`. Must be called from
// the PM sequence.
scoped_refptr<RefCountedScenarioState> GetSharedStateForProcessNode(
    const ProcessNode* process_node) {
  // Returns a copy of the pointer.
  return GetOrCreatePerformanceScenarioMemoryData(
             ProcessNodeImpl::FromNode(process_node))
      .state_ptr;
}

// Returns a pointer to the global shared memory region that can be read by all
// processes, or nullptr if none exists. GlobalPerformanceScenarioMemory
// manages the lifetime of the region.
scoped_refptr<RefCountedScenarioState> GetGlobalSharedState() {
  // Returns a copy of the pointer.
  return GlobalSharedStatePtr();
}

// Convenience accessors to return references to the correct member of `state`
// for Scenario.
template <typename Scenario>
std::atomic<Scenario>& GetScenarioRef(SharedScenarioState& state);

template <>
std::atomic<LoadingScenario>& GetScenarioRef(SharedScenarioState& state) {
  return state.WritableRef().loading;
}

template <>
std::atomic<InputScenario>& GetScenarioRef(SharedScenarioState& state) {
  return state.WritableRef().input;
}

template <typename Scenario>
void SetScenarioValue(Scenario scenario,
                      scoped_refptr<RefCountedScenarioState> state_ptr) {
  if (state_ptr) {
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    GetScenarioRef<Scenario>(state_ptr->shared_state())
        .store(scenario, std::memory_order_relaxed);
  }
}

template <typename Scenario>
void SetScenarioValueForProcessNode(Scenario scenario,
                                    const ProcessNode* process_node) {
  SetScenarioValue(scenario, GetSharedStateForProcessNode(process_node));
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
              SetScenarioValue(
                  scenario, GetSharedStateForProcessNode(process_node.get()));
            }
          },
          scenario,
          PerformanceManager::GetProcessNodeForRenderProcessHost(host)));
}

template <typename Scenario>
void SetGlobalScenarioValue(Scenario scenario) {
  SetScenarioValue(scenario, GetGlobalSharedState());
}

}  // namespace

ScopedGlobalScenarioMemory::ScopedGlobalScenarioMemory() {
  CHECK(!GlobalSharedStatePtr());
  GlobalSharedStatePtr() = RefCountedScenarioState::Create();
  read_only_mapping_.emplace(blink::performance_scenarios::Scope::kGlobal,
                             GetGlobalSharedScenarioRegion());
}

ScopedGlobalScenarioMemory::~ScopedGlobalScenarioMemory() {
  GlobalSharedStatePtr().reset();
}

base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcessNode(
    const ProcessNode* process_node) {
  auto state_ptr = GetSharedStateForProcessNode(process_node);
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
