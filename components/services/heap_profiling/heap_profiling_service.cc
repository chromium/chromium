// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/heap_profiling_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "components/services/heap_profiling/connection_manager.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {

namespace {

class ProfilingServiceImpl;

class ProfilingServiceImpl
    : public mojom::ProfilingService,
      public memory_instrumentation::mojom::HeapProfiler {
 public:
  ProfilingServiceImpl(
      mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler>
          profiler_receiver,
      mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
          helper)
      : heap_profiler_receiver_(this, std::move(profiler_receiver)),
        helper_(std::move(helper)) {}

  ~ProfilingServiceImpl() override = default;

  // mojom::ProfilingService implementation:
  void AddProfilingClient(base::ProcessId pid,
                          mojo::PendingRemote<mojom::ProfilingClient> client,
                          mojom::ProcessType process_type,
                          mojom::ProfilingParamsPtr params) override {
    if (params->sampling_rate == 0)
      params->sampling_rate = 1;
    connection_manager_.OnNewConnection(pid, std::move(client), process_type,
                                        std::move(params));
  }

  void GetProfiledPids(GetProfiledPidsCallback callback) override {
    std::move(callback).Run(connection_manager_.GetConnectionPids());
  }

  // memory_instrumentation::mojom::HeapProfiler implementation:
  void DumpProcessesForTracing(
      bool strip_path_from_mapped_files,
      DumpProcessesForTracingCallback callback) override {
    std::vector<base::ProcessId> pids =
        connection_manager_.GetConnectionPidsThatNeedVmRegions();
    if (pids.empty()) {
      connection_manager_.DumpProcessesForTracing(
          strip_path_from_mapped_files, std::move(callback), VmRegions());
      return;
    }

    // Need a memory map to make sense of the dump. The dump will be triggered
    // in the memory map global dump callback.
    helper_->GetVmRegionsForHeapProfiler(
        pids,
        base::BindOnce(&ProfilingServiceImpl::
                           OnGetVmRegionsCompleteForDumpProcessesForTracing,
                       weak_factory_.GetWeakPtr(), strip_path_from_mapped_files,
                       std::move(callback)));
  }

 private:
  void OnGetVmRegionsCompleteForDumpProcessesForTracing(
      bool strip_path_from_mapped_files,
      DumpProcessesForTracingCallback callback,
      VmRegions vm_regions) {
    connection_manager_.DumpProcessesForTracing(strip_path_from_mapped_files,
                                                std::move(callback),
                                                std::move(vm_regions));
  }

  mojo::Receiver<memory_instrumentation::mojom::HeapProfiler>
      heap_profiler_receiver_{this};
  mojo::Remote<memory_instrumentation::mojom::HeapProfilerHelper> helper_;
  ConnectionManager connection_manager_;

  base::WeakPtrFactory<ProfilingServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilingServiceImpl);
};

void RunHeapProfilingService(
    mojo::PendingReceiver<mojom::ProfilingService> receiver,
    mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler>
        profiler_receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
        helper) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ProfilingServiceImpl>(std::move(profiler_receiver),
                                             std::move(helper)),
      std::move(receiver));
}

}  // namespace

mojo::PendingRemote<mojom::ProfilingService> LaunchService(
    mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler>
        profiler_receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
        helper) {
  mojo::PendingRemote<mojom::ProfilingService> remote;
  auto task_runner = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
       base::WithBaseSyncPrimitives()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&RunHeapProfilingService,
                     remote.InitWithNewPipeAndPassReceiver(),
                     std::move(profiler_receiver), std::move(helper)));
  return remote;
}

}  // namespace heap_profiling
