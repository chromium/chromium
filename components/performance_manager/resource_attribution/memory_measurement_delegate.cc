// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace resource_attribution {

namespace {

using GlobalMemoryDump = memory_instrumentation::GlobalMemoryDump;

// A production MemoryMeasurementDelegate that measures processes with
// memory_instrumentation::MemoryInstrumentation.
class MemoryMeasurementDelegateImpl final : public MemoryMeasurementDelegate {
 public:
  explicit MemoryMeasurementDelegateImpl(Graph* graph)
      : graph_impl_(GraphImpl::FromGraph(graph)) {}

  ~MemoryMeasurementDelegateImpl() final = default;

  MemoryMeasurementDelegateImpl(const MemoryMeasurementDelegateImpl&) = delete;
  MemoryMeasurementDelegateImpl& operator=(
      const MemoryMeasurementDelegateImpl&) = delete;

  void RequestMemorySummary(
      base::OnceCallback<void(MemorySummaryMap)> callback) final;

 private:
  void OnMemorySummary(base::OnceCallback<void(MemorySummaryMap)> callback,
                       bool success,
                       std::unique_ptr<GlobalMemoryDump> memory_dump);

  raw_ptr<GraphImpl> graph_impl_;
  base::WeakPtrFactory<MemoryMeasurementDelegateImpl> weak_factory_{this};
};

void MemoryMeasurementDelegateImpl::RequestMemorySummary(
    base::OnceCallback<void(MemorySummaryMap)> callback) {
  auto* mem_instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  // The memory instrumentation service is not available in unit tests unless
  // explicitly created.
  if (!mem_instrumentation) {
    std::move(callback).Run({});
    return;
  }
  // TODO(crbug.com/40926264): Pass a set of processes to measure instead of
  // all?
  mem_instrumentation->RequestPrivateMemoryFootprint(
      base::kNullProcessId,
      base::BindOnce(&MemoryMeasurementDelegateImpl::OnMemorySummary,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MemoryMeasurementDelegateImpl::OnMemorySummary(
    base::OnceCallback<void(MemorySummaryMap)> callback,
    bool success,
    std::unique_ptr<GlobalMemoryDump> memory_dump) {
  if (!success) {
    std::move(callback).Run({});
    return;
  }
  MemorySummaryMap results;
  CHECK(memory_dump);
  for (const auto& process_dump : memory_dump->process_dumps()) {
    ProcessNodeImpl* process_node =
        graph_impl_->GetProcessNodeByPid(process_dump.pid());
    if (!process_node) {
      // TODO(crbug.com/40926264): Save ProcessContext by PID when the request
      // starts, so that ProcessNode's deleted before the result task runs can
      // be measured?
      continue;
    }
    results.emplace(
        ProcessContext::FromProcessNode(process_node),
        MemorySummaryMeasurement{
            .resident_set_size_kb = process_dump.os_dump().resident_set_kb,
            .private_footprint_kb = process_dump.os_dump().private_footprint_kb,
        });
  }
  std::move(callback).Run(std::move(results));
}

// The default production factory for MemoryMeasurementDelegateImpl objects.
class MemoryMeasurementDelegateFactoryImpl final
    : public MemoryMeasurementDelegate::Factory {
 public:
  MemoryMeasurementDelegateFactoryImpl() = default;
  ~MemoryMeasurementDelegateFactoryImpl() final = default;

  std::unique_ptr<MemoryMeasurementDelegate> CreateDelegate(
      Graph* graph) final {
    return std::make_unique<MemoryMeasurementDelegateImpl>(graph);
  }
};

}  // namespace

// static
void MemoryMeasurementDelegate::SetDelegateFactoryForTesting(Graph* graph,
                                                             Factory* factory) {
  auto* scheduler = internal::QueryScheduler::GetFromGraph(graph);
  CHECK(scheduler);
  scheduler
      ->GetMemoryProviderForTesting()                  // IN-TEST
      .SetDelegateFactoryForTesting(factory ? factory  // IN-TEST
                                            : GetDefaultFactory());
}

// static
MemoryMeasurementDelegate::Factory*
MemoryMeasurementDelegate::GetDefaultFactory() {
  static base::NoDestructor<MemoryMeasurementDelegateFactoryImpl>
      default_factory;
  return default_factory.get();
}

}  // namespace resource_attribution
