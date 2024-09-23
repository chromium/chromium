// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/connection_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_macros.h"
#include "components/services/heap_profiling/json_exporter.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"

namespace heap_profiling {

// Tracking information for DumpProcessForTracing(). This struct is
// refcounted since there will be many background thread calls (one for each
// AllocationTracker) and the callback is only issued when each has
// responded.
//
// This class is not threadsafe, its members must only be accessed on the
// I/O thread.
struct ConnectionManager::DumpProcessesForTracingTracking
    : public base::RefCountedThreadSafe<DumpProcessesForTracingTracking> {
  // Number of processes we're still waiting on responses for. When this gets
  // to 0, the callback will be issued.
  size_t waiting_responses = 0;

  // Callback to issue when dumps are complete.
  DumpProcessesForTracingCallback callback;

  // Info about the request.
  VmRegions vm_regions;

  // Collects the results.
  std::vector<memory_instrumentation::mojom::HeapProfileResultPtr> results;

 private:
  friend class base::RefCountedThreadSafe<DumpProcessesForTracingTracking>;
  virtual ~DumpProcessesForTracingTracking() = default;
};

struct ConnectionManager::Connection {
  Connection(CompleteCallback complete_cb,
             mojo::PendingRemote<mojom::ProfilingClient> client,
             mojom::ProcessType process_type,
             uint32_t sampling_rate,
             mojom::StackMode stack_mode,
             mojom::ProfilingService::AddProfilingClientCallback
                 started_profiling_callback)
      : client(std::move(client)),
        process_type(process_type),
        stack_mode(stack_mode),
        sampling_rate(sampling_rate),
        started_profiling_callback(std::move(started_profiling_callback)) {
    this->client.set_disconnect_handler(std::move(complete_cb));
  }

  bool HeapDumpNeedsVmRegions() {
    return stack_mode == mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES ||
           stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES;
  }

  mojo::Remote<mojom::ProfilingClient> client;
  mojom::ProcessType process_type;
  mojom::StackMode stack_mode;

  bool started_profiling = false;

  // When sampling is enabled, allocations are recorded with probability (size /
  // sampling_rate) when size < sampling_rate. When size >= sampling_rate, the
  // aggregate probability of an allocation being recorded is 1.0, but the math
  // and details are tricky. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=810748#c4.
  // A |sampling_rate| of 1 is equivalent to recording all allocations.
  uint32_t sampling_rate = 1;

  mojom::ProfilingService::AddProfilingClientCallback
      started_profiling_callback;
};

ConnectionManager::ConnectionManager() {
  metrics_timer_.Start(FROM_HERE, base::Hours(24),
                       base::BindRepeating(&ConnectionManager::ReportMetrics,
                                           base::Unretained(this)));
}
ConnectionManager::~ConnectionManager() = default;

void ConnectionManager::OnNewConnection(
    base::ProcessId pid,
    mojo::PendingRemote<mojom::ProfilingClient> client,
    mojom::ProcessType process_type,
    mojom::ProfilingParamsPtr params,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  base::AutoLock lock(connections_lock_);

  // Attempting to start profiling on an already profiled processs should have
  // no effect.
  if (connections_.find(pid) != connections_.end()) {
    std::move(started_profiling_closure).Run(/*success=*/false);
    return;
  }

  // It's theoretically possible that we started profiling a process, the
  // profiling was stopped [e.g. by hitting the 10-s timeout], and then we tried
  // to start profiling again. The ProfilingClient will refuse to start again.
  // But the ConnectionManager will not be able to distinguish this
  // never-started ProfilingClient from a brand new ProfilingClient that happens
  // to share the same pid. This is a rare condition which should only happen
  // when the user is attempting to manually start profiling for processes, so
  // we ignore this edge case.

  CompleteCallback complete_cb =
      base::BindOnce(&ConnectionManager::OnConnectionComplete,
                     weak_factory_.GetWeakPtr(), pid);

  auto connection = std::make_unique<Connection>(
      std::move(complete_cb), std::move(client), process_type,
      params->sampling_rate, params->stack_mode,
      std::move(started_profiling_closure));
  connection->client->StartProfiling(
      std::move(params), base::BindOnce(&ConnectionManager::OnProfilingStarted,
                                        weak_factory_.GetWeakPtr(), pid));
  connections_[pid] = std::move(connection);
}

std::vector<base::ProcessId> ConnectionManager::GetConnectionPids() {
  base::AutoLock lock(connections_lock_);
  std::vector<base::ProcessId> results;
  results.reserve(connections_.size());
  for (const auto& pair : connections_) {
    if (pair.second->started_profiling)
      results.push_back(pair.first);
  }
  return results;
}

std::vector<base::ProcessId>
ConnectionManager::GetConnectionPidsThatNeedVmRegions() {
  base::AutoLock lock(connections_lock_);
  std::vector<base::ProcessId> results;
  results.reserve(connections_.size());
  for (const auto& pair : connections_) {
    if (pair.second->HeapDumpNeedsVmRegions())
      results.push_back(pair.first);
  }
  return results;
}

void ConnectionManager::OnConnectionComplete(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  auto found = connections_.find(pid);
  CHECK(found != connections_.end());
  if (!found->second->started_profiling_callback.is_null()) {
    std::move(found->second->started_profiling_callback).Run(/*success=*/false);
  }
  connections_.erase(found);
}

void ConnectionManager::OnProfilingStarted(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);

  // It's possible that the client disconnected in the short time before
  // profiling started.
  auto found = connections_.find(pid);
  if (found != connections_.end()) {
    found->second->started_profiling = true;
    std::move(found->second->started_profiling_callback).Run(/*success=*/true);
  }
}

void ConnectionManager::ReportMetrics() {
  base::AutoLock lock(connections_lock_);
  for (auto& pair : connections_) {
    UMA_HISTOGRAM_ENUMERATION("HeapProfiling.ProfiledProcess.Type",
                              pair.second->process_type,
                              static_cast<int>(mojom::ProcessType::LAST) + 1);
  }
}

void ConnectionManager::DumpProcessesForTracing(
    bool strip_path_from_mapped_files,
    bool write_proto,
    DumpProcessesForTracingCallback callback,
    VmRegions vm_regions) {
  base::AutoLock lock(connections_lock_);

  // Early out if there are no connections.
  if (connections_.empty()) {
    std::move(callback).Run(
        std::vector<memory_instrumentation::mojom::HeapProfileResultPtr>());
    return;
  }

  auto tracking = base::MakeRefCounted<DumpProcessesForTracingTracking>();
  tracking->waiting_responses = connections_.size();
  tracking->callback = std::move(callback);
  tracking->vm_regions = std::move(vm_regions);
  tracking->results.reserve(connections_.size());

  for (auto& it : connections_) {
    base::ProcessId pid = it.first;
    Connection* connection = it.second.get();
    // TODO(ssid): Stop writing JSON to traces when proto output is enabled,
    // https://crbug.com/1228548.
    if (write_proto)
      connection->client->AddHeapProfileToTrace(base::DoNothing());

    connection->client->RetrieveHeapProfile(base::BindOnce(
        &ConnectionManager::HeapProfileRetrieved, weak_factory_.GetWeakPtr(),
        tracking, pid, connection->process_type, strip_path_from_mapped_files,
        connection->sampling_rate));
  }
}

bool ConnectionManager::ConvertProfileToExportParams(
    mojom::HeapProfilePtr profile,
    uint32_t sampling_rate,
    ExportParams* params) {
  AllocationMap allocs;
  ContextMap context_map;
  AddressToStringMap string_map;

  for (const mojom::HeapProfileSamplePtr& sample : profile->samples) {
    int context_id = 0;
    if (sample->context_id) {
      auto it = profile->strings.find(sample->context_id);
      if (it == profile->strings.end())
        return false;
      const std::string& context = it->second;
      // Escape the strings early, to simplify exporting a heap dump.
      std::string escaped_context;
      base::EscapeJSONString(context, false /* put_in_quotes */,
                             &escaped_context);
      context_id = context_map
                       .emplace(std::move(escaped_context),
                                static_cast<int>(context_map.size() + 1))
                       .first->second;
    }

    size_t alloc_size = sample->total;
    float alloc_count = 1;
    if (sample->size != 0)
      alloc_count = float(sample->total) / float(sample->size);

    std::vector<Address> stack(sample->stack.begin(), sample->stack.end());
    AllocationMetrics& metrics =
        allocs
            .emplace(std::piecewise_construct,
                     std::forward_as_tuple(sample->allocator, std::move(stack),
                                           context_id),
                     std::forward_as_tuple())
            .first->second;
    metrics.size += alloc_size;
    metrics.count += alloc_count;
  }

  for (const auto& str : profile->strings) {
    std::string quoted_string;
    // Escape the strings before saving them, to simplify exporting a heap dump.
    base::EscapeJSONString(str.second, false /* put_in_quotes */,
                           &quoted_string);
    string_map.emplace(str.first, std::move(quoted_string));
  }

  params->allocs = std::move(allocs);
  params->context_map = std::move(context_map);
  params->mapped_strings = std::move(string_map);
  return true;
}

void ConnectionManager::HeapProfileRetrieved(
    scoped_refptr<DumpProcessesForTracingTracking> tracking,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    bool strip_path_from_mapped_files,
    uint32_t sampling_rate,
    mojom::HeapProfilePtr profile) {
  // All code paths through here must issue the callback when waiting_responses
  // is 0 or the browser will wait forever for the dump.
  DCHECK(tracking->waiting_responses > 0);

  ExportParams params;
  bool success =
      ConvertProfileToExportParams(std::move(profile), sampling_rate, &params);
  if (success) {
    params.process_type = process_type;
    params.strip_path_from_mapped_files = strip_path_from_mapped_files;
    params.next_id = next_id_;

    auto it = tracking->vm_regions.find(pid);
    if (it != tracking->vm_regions.end())
      params.maps = std::move(it->second);

    memory_instrumentation::mojom::HeapProfileResultPtr result =
        memory_instrumentation::mojom::HeapProfileResult::New();
    result->pid = pid;
    result->json = ExportMemoryMapsAndV2StackTraceToJSON(&params);
    tracking->results.push_back(std::move(result));
    next_id_ = params.next_id;
  }

  // When all responses complete, issue done callback.
  if (--tracking->waiting_responses == 0)
    std::move(tracking->callback).Run(std::move(tracking->results));
}

}  // namespace heap_profiling
