// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/allocation_trace_recorder_statistics_reporter.h"

#include <functional>
#include <ios>
#include <string>
#include <string_view>
#include <utility>

#include "base/debug/allocation_trace.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

using base::debug::tracer::AllocationTraceRecorder;

namespace memory_system::internal {

class AllocationTraceRecorderStatisticsReporter::Core {
 public:
  Core(const AllocationTraceRecorder& recorder,
       std::string process_type,
       base::TimeDelta interval,
       logging::LogSeverity severity);

 private:
  static void LogRecorderStatistics(const AllocationTraceRecorder& recorder,
                                    const std::string& process_type,
                                    logging::LogSeverity severity);

  base::RepeatingTimer timer_;
};

AllocationTraceRecorderStatisticsReporter::Core::Core(
    const AllocationTraceRecorder& recorder,
    std::string process_type,
    base::TimeDelta interval,
    logging::LogSeverity severity) {
  timer_.Start(
      FROM_HERE, interval,
      base::BindRepeating(&Core::LogRecorderStatistics, std::cref(recorder),
                          std::move(process_type), severity));
}

void AllocationTraceRecorderStatisticsReporter::Core::LogRecorderStatistics(
    const AllocationTraceRecorder& recorder,
    const std::string& process_type,
    logging::LogSeverity severity) {
  const auto recorder_stats = recorder.GetRecorderStatistics();
  const float collision_ratio =
      recorder_stats.total_number_of_allocations != 0
          ? static_cast<float>(recorder_stats.total_number_of_collisions) /
                recorder_stats.total_number_of_allocations
          : 0;

  logging::LogMessage(__FILE__, __LINE__, severity).stream()
      << "process-type=" << process_type << ", number_of_allocations="
      << recorder_stats.total_number_of_allocations
      << ", number_of_collisions=" << recorder_stats.total_number_of_collisions
      << ", collision_ratio = " << std::fixed << collision_ratio;
}

AllocationTraceRecorderStatisticsReporter::
    AllocationTraceRecorderStatisticsReporter() = default;

AllocationTraceRecorderStatisticsReporter::
    AllocationTraceRecorderStatisticsReporter(
        const base::debug::tracer::AllocationTraceRecorder& recorder,
        std::string_view process_type,
        base::TimeDelta interval,
        logging::LogSeverity severity)
    : core_(base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
            std::cref(recorder),
            std::string(process_type),
            interval,
            severity) {}

AllocationTraceRecorderStatisticsReporter::
    AllocationTraceRecorderStatisticsReporter(
        AllocationTraceRecorderStatisticsReporter&&) = default;
AllocationTraceRecorderStatisticsReporter&
AllocationTraceRecorderStatisticsReporter::operator=(
    AllocationTraceRecorderStatisticsReporter&&) = default;

AllocationTraceRecorderStatisticsReporter::
    ~AllocationTraceRecorderStatisticsReporter() = default;

}  // namespace memory_system::internal
