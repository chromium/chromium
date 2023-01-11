// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/memory_metrics_logger.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

using memory_instrumentation::GetPrivateFootprintHistogramName;
using memory_instrumentation::HistogramProcessType;

namespace metrics {
namespace {

MemoryMetricsLogger* g_instance = nullptr;

// Called once the metrics have been determined. Does the actual logging.
void RecordMemoryMetricsImpl(
    MemoryMetricsLogger::RecordCallback done_callback,
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  if (!success) {
    if (done_callback)
      std::move(done_callback).Run(false);
    return;
  }

  uint64_t total_private_footprint_kb = 0;
  for (const auto& process_dump : dump->process_dumps()) {
    total_private_footprint_kb += process_dump.os_dump().private_footprint_kb;
    switch (process_dump.process_type()) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kBrowser),
            process_dump.os_dump().private_footprint_kb / 1024);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        // On the desktop this may be attributed to an 'extension', but as
        // android doesn't support extensions there is no checking.
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kRenderer),
            process_dump.os_dump().private_footprint_kb / 1024);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::GPU: {
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kGpu),
            process_dump.os_dump().private_footprint_kb / 1024);
        break;
      }

      // Currently this class only records metrics for the browser and
      // renderer process, as it originated from WebView, where there are no
      // other processes.
      case memory_instrumentation::mojom::ProcessType::ARC:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::UTILITY:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::PLUGIN:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::OTHER:
        break;
    }
  }
  if (total_private_footprint_kb) {
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.PrivateMemoryFootprint",
                                total_private_footprint_kb / 1024);
  }
  if (done_callback)
    std::move(done_callback).Run(true);
}

}  // namespace

// State is used to trigger logging to stop. State is accessed on both the main
// thread and the background task runner.
struct MemoryMetricsLogger::State : public base::RefCountedThreadSafe<State> {
  State() = default;

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  // MemoryInstrumentation requires a SequencedTaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  bool stop_logging = false;

 private:
  friend class base::RefCountedThreadSafe<State>;

  ~State() = default;
};

MemoryMetricsLogger::MemoryMetricsLogger()
    : state_(base::MakeRefCounted<State>()) {
  g_instance = this;
  state_->task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  state_->task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetricsAfterDelay,
                     state_));
}

MemoryMetricsLogger::~MemoryMetricsLogger() {
  g_instance = nullptr;
  state_->stop_logging = true;
}

// static
MemoryMetricsLogger* MemoryMetricsLogger::GetInstanceForTesting() {
  return g_instance;
}

void MemoryMetricsLogger::ScheduleRecordForTesting(
    RecordCallback done_callback) {
  state_->task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetrics,
                                state_, std::move(done_callback)));
}

// static
void MemoryMetricsLogger::RecordMemoryMetricsAfterDelay(
    scoped_refptr<State> state) {
  if (state->stop_logging)
    return;

  state->task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetrics, state,
                     RecordCallback()),
      memory_instrumentation::GetDelayForNextMemoryLog());
}

// static
void MemoryMetricsLogger::RecordMemoryMetrics(scoped_refptr<State> state,
                                              RecordCallback done_callback) {
  auto* instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  if (!instrumentation) {
    // Content layer is not initialized yet, nothing to log.
    return;
  }
  instrumentation->RequestGlobalDump(
      {}, base::BindOnce(&RecordMemoryMetricsImpl, std::move(done_callback)));
  RecordMemoryMetricsAfterDelay(state);
}

}  // namespace metrics
