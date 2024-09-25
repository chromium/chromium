// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

namespace {

using SharedScenarioState = blink::performance_scenarios::SharedScenarioState;

// Pointers to the mapped shared memory are held in thread-safe scoped_refptr's.
// The memory will be unmapped when the final reference is dropped. Functions
// that write to the shared memory must hold a reference to it so that it's not
// unmapped while writing.
using RefCountedScenarioMemory = base::RefCountedData<SharedScenarioState>;

// Holds the browser's scenario state handle for a child's scenario state.
class ProcessUserData final : public base::SupportsUserData::Data {
 public:
  ~ProcessUserData() final = default;

  scoped_refptr<RefCountedScenarioMemory> shared_mem;

  static const char kKey[];
};

const char ProcessUserData::kKey[] = "performance_manager::ProcessUserData";

// Holds the browser's global scenario state handle.
scoped_refptr<RefCountedScenarioMemory>& GlobalSharedMemPtr() {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMemory>> shared_mem;
  return *shared_mem;
}

// Returns a pointer to the shared memory region for communicating private state
// to the process hosted in `host`. Creates a region if none exists yet,
// returning nullptr on failure. The region's lifetime is tied to `host`.
scoped_refptr<RefCountedScenarioMemory> GetScenarioMemoryForProcess(
    content::RenderProcessHost& host) {
  ProcessUserData* data =
      static_cast<ProcessUserData*>(host.GetUserData(&ProcessUserData::kKey));
  if (!data) {
    // Create a new shared memory region to communicate private state for the
    // child process. The region will be destroyed when `host` is deleted.
    auto new_data = std::make_unique<ProcessUserData>();
    data = new_data.get();
    host.SetUserData(&ProcessUserData::kKey, std::move(new_data));
    std::optional<SharedScenarioState> shared_state =
        SharedScenarioState::Create();
    if (shared_state.has_value()) {
      data->shared_mem = base::MakeRefCounted<RefCountedScenarioMemory>(
          std::move(shared_state.value()));
    }
  }
  // Returns a copy of the pointer.
  return data->shared_mem;
}

// Returns a pointer to the global shared memory region that can be read by all
// processes, or nullptr if none exists. GlobalPerformanceScenarioMemory/
// manages the lifetime of the region.
scoped_refptr<RefCountedScenarioMemory> GetGlobalScenarioMemory() {
  // Returns a copy of the pointer.
  return GlobalSharedMemPtr();
}

void SetLoadingScenario(scoped_refptr<RefCountedScenarioMemory> shared_mem,
                        LoadingScenario scenario) {
  if (shared_mem) {
    // std::memory_order_relaxed is sufficient since no other memory depends on
    // the scenario value.
    shared_mem->data.WritableRef().loading.store(scenario,
                                                 std::memory_order_relaxed);
  }
}

void SetInputScenario(scoped_refptr<RefCountedScenarioMemory> shared_mem,
                      InputScenario scenario) {
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

base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcess(
    content::RenderProcessHost& host) {
  auto shared_mem = GetScenarioMemoryForProcess(host);
  return shared_mem ? shared_mem->data.DuplicateReadOnlyRegion()
                    : base::ReadOnlySharedMemoryRegion();
}

base::ReadOnlySharedMemoryRegion GetGlobalSharedScenarioRegion() {
  auto shared_mem = GetGlobalScenarioMemory();
  return shared_mem ? shared_mem->data.DuplicateReadOnlyRegion()
                    : base::ReadOnlySharedMemoryRegion();
}

void SetLoadingScenarioForProcess(content::RenderProcessHost& host,
                                  LoadingScenario scenario) {
  SetLoadingScenario(GetScenarioMemoryForProcess(host), scenario);
}

void SetGlobalLoadingScenario(LoadingScenario scenario) {
  SetLoadingScenario(GetGlobalScenarioMemory(), scenario);
}

void SetInputScenarioForProcess(content::RenderProcessHost& host,
                                InputScenario scenario) {
  SetInputScenario(GetScenarioMemoryForProcess(host), scenario);
}

void SetGlobalInputScenario(InputScenario scenario) {
  SetInputScenario(GetGlobalScenarioMemory(), scenario);
}

}  // namespace performance_manager
