// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/heap_profiling_trace_source.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/profiler/frame.h"
#include "base/profiler/module_cache.h"
#include "base/trace_event/trace_event.h"
#include "components/services/heap_profiling/public/cpp/merge_samples.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace heap_profiling {

std::string GetHeapName(
    base::allocator::dispatcher::AllocationSubsystem allocator_type) {
  using base::allocator::dispatcher::AllocationSubsystem;
  switch (allocator_type) {
    case AllocationSubsystem::kAllocatorShim:
      return "malloc";
    case AllocationSubsystem::kPartitionAllocator:
      return "partition_alloc";
    case AllocationSubsystem::kManualForTesting:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

HeapProfilingTraceSource::HeapProfilingTraceSource()
    : DataSourceBase(tracing::mojom::kNativeHeapProfilerSourceName) {
  tracing::PerfettoTracedProcess::Get()->AddDataSource(this);
}

HeapProfilingTraceSource::~HeapProfilingTraceSource() = default;

// static
HeapProfilingTraceSource* HeapProfilingTraceSource::GetInstance() {
  static base::NoDestructor<HeapProfilingTraceSource> instance;
  return instance.get();
}

void HeapProfilingTraceSource::StartTracingImpl(
    tracing::PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  base::AutoLock l(lock_);
  trace_writer_ =
      producer->CreateTraceWriter(data_source_config.target_buffer());
  stack_profiler_writer_ =
      std::make_unique<tracing::TracingSamplerProfiler::StackProfileWriter>(
          data_source_config.chrome_config().privacy_filtering_enabled());
}

void HeapProfilingTraceSource::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  base::AutoLock l(lock_);
  stack_profiler_writer_.reset();
  trace_writer_.reset();
  std::move(stop_complete_callback).Run();
}

void HeapProfilingTraceSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  flush_complete_callback.Run();
}

bool HeapProfilingTraceSource::AddToTraceIfEnabled(
    const std::vector<base::SamplingHeapProfiler::Sample>& samples) {
  base::AutoLock l(lock_);
  if (!trace_writer_)
    return false;

  base::ModuleCache module_cache;

  uint64_t timestamp = TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds();

  stack_profiler_writer_->ResetEmittedState();

  {
    // Write initial packet on sequence to specify incremental state is reset.
    perfetto::TraceWriter::TracePacketHandle trace_packet;
    trace_packet = trace_writer_->NewTracePacket();

    trace_packet->set_timestamp(timestamp);
    trace_packet->set_incremental_state_cleared(true);
  }

  // Write each sample in a new packet to keep the packet size small.
  auto merged_samples = MergeSamples(samples);

  uint32_t pid = base::GetCurrentProcId();
  uint64_t index = 0;
  for (auto& pair : merged_samples) {
    perfetto::TraceWriter::TracePacketHandle trace_packet;
    trace_packet = trace_writer_->NewTracePacket();

    trace_packet->set_timestamp(timestamp);

    const base::SamplingHeapProfiler::Sample& sample = pair.first;
    const SampleValue& value = pair.second;

    std::vector<base::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    tracing::InterningID callstack_id =
        stack_profiler_writer_->GetCallstackIDAndMaybeEmit(frames,
                                                           &trace_packet);

    auto* profile_packet = trace_packet->set_profile_packet();
    profile_packet->set_continued(true);
    profile_packet->set_index(index);
    index++;
    auto* process_dump = profile_packet->add_process_dumps();
    process_dump->set_pid(pid);
    process_dump->set_heap_name(GetHeapName(sample.allocator));
    process_dump->set_timestamp(timestamp);

    auto* sample_proto = process_dump->add_samples();
    sample_proto->set_self_allocated(value.total);
    sample_proto->set_alloc_count(value.count);
    sample_proto->set_callstack_id(callstack_id);
  }

  {
    // Write the end packet with continued=false to mark the current profile
    // done.
    perfetto::TraceWriter::TracePacketHandle trace_packet;
    trace_packet = trace_writer_->NewTracePacket();

    trace_packet->set_timestamp(timestamp);

    auto* profile_packet = trace_packet->set_profile_packet();
    profile_packet->set_continued(false);
    profile_packet->set_index(index);
  }
  return true;
}

}  // namespace heap_profiling
