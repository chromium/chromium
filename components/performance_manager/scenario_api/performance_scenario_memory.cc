// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenario_memory.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_scenarios {

namespace {

// Global pointers to the shared memory mappings. Once a thread has a copy of
// one of these pointers, it can manipulate the refcount atomically, so doesn't
// have to worry about the underlying ScenarioMapping disappearing. But the
// scoped_refptr itself is not atomic so the corresponding
// MappingPtrLockForScope must be held to get that copy.
scoped_refptr<RefCountedScenarioMapping>& MappingPtrForScope(
    ScenarioScope scope) {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMapping>>
      current_process_mapping;
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMapping>>
      global_mapping;
  switch (scope) {
    case ScenarioScope::kCurrentProcess:
      return *current_process_mapping;
    case ScenarioScope::kGlobal:
      return *global_mapping;
  }
  NOTREACHED();
}

base::Lock& MappingPtrLockForScope(ScenarioScope scope) {
  static base::NoDestructor<base::Lock> current_process_lock;
  static base::NoDestructor<base::Lock> global_lock;
  switch (scope) {
    case ScenarioScope::kCurrentProcess:
      return *current_process_lock;
    case ScenarioScope::kGlobal:
      return *global_lock;
  }
  NOTREACHED();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ChildScenarioMappingResult)
enum class MappingResult {
  kSuccess = 0,
  kInvalidHandle = 1,
  kSystemError = 2,
  kMaxValue = kSystemError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/performance_manager/enums.xml:ChildScenarioMappingResult)

void LogMappingResult(
    MappingResult result,
    std::optional<logging::SystemErrorCode> system_error = std::nullopt) {
  base::UmaHistogramEnumeration("PerformanceManager.ChildScenarioMappingResult",
                                result);
  if (system_error.has_value()) {
    base::UmaHistogramSparse(
        "PerformanceManager.ChildScenarioMappingSystemError",
        system_error.value());
  }
}

}  // namespace

// TODO(crbug.com/365586676): Currently these are only mapped into browser and
// renderer processes. The global scenarios should also be mapped into utility
// processes.

ScopedReadOnlyScenarioMemory::ScopedReadOnlyScenarioMemory(
    ScenarioScope scope,
    base::ReadOnlySharedMemoryRegion region)
    : scope_(scope) {
  using SharedScenarioState = base::StructuredSharedMemory<ScenarioState>;
  if (!region.IsValid()) {
    LogMappingResult(MappingResult::kInvalidHandle);
  } else if (std::optional<SharedScenarioState::ReadOnlyMapping> mapping =
                 SharedScenarioState::MapReadOnlyRegion(std::move(region))) {
    base::AutoLock lock(MappingPtrLockForScope(scope_));
    MappingPtrForScope(scope_) =
        base::MakeRefCounted<RefCountedScenarioMapping>(
            std::move(mapping.value()));
    LogMappingResult(MappingResult::kSuccess);
  } else {
    LogMappingResult(MappingResult::kSystemError,
                     logging::GetLastSystemErrorCode());
  }

  // The ObserverList must be created after mapping the memory, because it reads
  // the scenario state in its constructor.
  PerformanceScenarioObserverList::CreateForScope(PassKey(), scope_);
}

ScopedReadOnlyScenarioMemory::~ScopedReadOnlyScenarioMemory() {
  PerformanceScenarioObserverList::DestroyForScope(PassKey(), scope_);
  base::AutoLock lock(MappingPtrLockForScope(scope_));
  MappingPtrForScope(scope_).reset();
}

scoped_refptr<RefCountedScenarioMapping> GetScenarioMappingForScope(
    ScenarioScope scope) {
  // This lock must be held while the scoped_refptr is copied.
  base::AutoLock lock(MappingPtrLockForScope(scope));
  return MappingPtrForScope(scope);
}

}  // namespace performance_scenarios
