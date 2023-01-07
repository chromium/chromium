// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_HEAP_PROFILING_TRACE_SOURCE_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_HEAP_PROFILING_TRACE_SOURCE_H_

#include <vector>

#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

namespace perfetto {
class TraceWriter;
}

namespace heap_profiling {

// Perfetto tracing data source for heap profiler. This writes the heap profiles
// to trace in proto format.
class HeapProfilingTraceSource
    : public tracing::PerfettoTracedProcess::DataSourceBase {
 public:
  static HeapProfilingTraceSource* GetInstance();

  // PerfettoTracedProcess::DataSourceBase implementation:
  void StartTracingImpl(
      tracing::PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracingImpl(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;

  // Writes heap profile to trace. Returns true if tracing was enabled and heap
  // profile was added to the trace, false otherwise.
  bool AddToTraceIfEnabled(
      const std::vector<base::SamplingHeapProfiler::Sample>& samples);

  HeapProfilingTraceSource(const HeapProfilingTraceSource&) = delete;
  HeapProfilingTraceSource& operator=(const HeapProfilingTraceSource&) = delete;

 private:
  friend class base::NoDestructor<HeapProfilingTraceSource>;

  HeapProfilingTraceSource();
  ~HeapProfilingTraceSource() override;

  // Start and stop calls are made from tracing sequence. AddToTrace() is called
  // on sequence used by ProfilingClient. It is possible that the tracing
  // sequence is blocked until heap profiles are finished when stopping. It is
  // ok to wait until profile is finished to stop tracing session.
  base::Lock lock_;
  std::unique_ptr<perfetto::TraceWriter> trace_writer_ GUARDED_BY(lock_);
  std::unique_ptr<tracing::TracingSamplerProfiler::StackProfileWriter>
      stack_profiler_writer_ GUARDED_BY(lock_);
};

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_HEAP_PROFILING_TRACE_SOURCE_H_
