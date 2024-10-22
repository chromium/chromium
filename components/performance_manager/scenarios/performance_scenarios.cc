// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
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
scoped_refptr<RefCountedScenarioMemory>& GlobalSharedMemPtr() {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMemory>> shared_mem;
  return *shared_mem;
}

// Returns a pointer to the shared memory region for communicating private state
// for `process_node`. Creates a region if none exists yet, returning nullptr on
// failure. The region's lifetime is tied to `process_node`. Must be called from
// the PM sequence.
scoped_refptr<RefCountedScenarioMemory> GetScenarioMemoryForProcessNode(
    const ProcessNode* process_node) {
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  if (PerformanceScenarioMemoryData::Exists(process_node_impl)) {
    // Returns a copy of the pointer.
    return PerformanceScenarioMemoryData::Get(process_node_impl).shared_mem;
  }
  // Create a new shared memory region to communicate private state for the
  // child process. The region will be destroyed when `process_node` is
  // deleted.
  auto& data = PerformanceScenarioMemoryData::Create(process_node_impl);
  std::optional<SharedScenarioState> shared_state =
      SharedScenarioState::Create();
  if (shared_state.has_value()) {
    data.shared_mem = base::MakeRefCounted<RefCountedScenarioMemory>(
        std::move(shared_state.value()));
  }
  // Returns a copy of the pointer.
  return data.shared_mem;
}

scoped_refptr<RefCountedScenarioMemory> GetScenarioMemoryForWeakProcessNode(
    base::WeakPtr<ProcessNode> process_node) {
  return process_node ? GetScenarioMemoryForProcessNode(process_node.get())
                      : nullptr;
}

// Returns a pointer to the global shared memory region that can be read by all
// processes, or nullptr if none exists. GlobalPerformanceScenarioMemory
// manages the lifetime of the region.
scoped_refptr<RefCountedScenarioMemory> GetGlobalScenarioMemory() {
  // Returns a copy of the pointer.
  return GlobalSharedMemPtr();
}

void SetLoadingScenario(LoadingScenario scenario,
                        scoped_refptr<RefCountedScenarioMemory> shared_mem) {
  if (shared_mem) {
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    shared_mem->data.WritableRef().loading.store(scenario,
                                                 std::memory_order_relaxed);
  }
}

void SetInputScenario(InputScenario scenario,
                      scoped_refptr<RefCountedScenarioMemory> shared_mem) {
  if (shared_mem) {
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    shared_mem->data.WritableRef().input.store(scenario,
                                               std::memory_order_relaxed);
  }
}

}  // namespace

ScopedGlobalScenarioMemory::ScopedGlobalScenarioMemory() {
  std::optional<SharedScenarioState> shared_state =
      SharedScenarioState::Create();
  if (shared_state.has_value()) {
    base::ReadOnlySharedMemoryRegion region =
        shared_state->DuplicateReadOnlyRegion();
    GlobalSharedMemPtr() = base::MakeRefCounted<RefCountedScenarioMemory>(
        std::move(shared_state.value()));
    read_only_mapping_.emplace(blink::performance_scenarios::Scope::kGlobal,
                               std::move(region));
  }
}

ScopedGlobalScenarioMemory::~ScopedGlobalScenarioMemory() {
  GlobalSharedMemPtr().reset();
}

base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcessNode(
    const ProcessNode* process_node) {
  auto shared_mem = GetScenarioMemoryForProcessNode(process_node);
  return shared_mem ? shared_mem->data.DuplicateReadOnlyRegion()
                    : base::ReadOnlySharedMemoryRegion();
}

base::ReadOnlySharedMemoryRegion GetGlobalSharedScenarioRegion() {
  auto shared_mem = GetGlobalScenarioMemory();
  return shared_mem ? shared_mem->data.DuplicateReadOnlyRegion()
                    : base::ReadOnlySharedMemoryRegion();
}

void SetLoadingScenarioForProcess(LoadingScenario scenario,
                                  content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          &GetScenarioMemoryForWeakProcessNode,
          PerformanceManager::GetProcessNodeForRenderProcessHost(host))
          .Then(base::BindOnce(&SetLoadingScenario, scenario)));
}

void SetLoadingScenarioForProcessNode(LoadingScenario scenario,
                                      const ProcessNode* process_node) {
  if (!process_node) {
    return;
  }
  SetLoadingScenario(scenario, GetScenarioMemoryForProcessNode(process_node));
}

void SetGlobalLoadingScenario(LoadingScenario scenario) {
  SetLoadingScenario(scenario, GetGlobalScenarioMemory());
}

void SetInputScenarioForProcess(InputScenario scenario,
                                content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          &GetScenarioMemoryForWeakProcessNode,
          PerformanceManager::GetProcessNodeForRenderProcessHost(host))
          .Then(base::BindOnce(&SetInputScenario, scenario)));
}

void SetInputScenarioForProcessNode(InputScenario scenario,
                                    const ProcessNode* process_node) {
  if (!process_node) {
    return;
  }
  SetInputScenario(scenario, GetScenarioMemoryForProcessNode(process_node));
}

void SetGlobalInputScenario(InputScenario scenario) {
  SetInputScenario(scenario, GetGlobalScenarioMemory());
}

}  // namespace performance_manager
