// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_cpu/cpu_probe_linux.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/system_cpu/core_times.h"
#include "components/system_cpu/cpu_sample.h"
#include "components/system_cpu/procfs_stat_cpu_parser.h"

namespace system_cpu {

// Helper class that performs the actual I/O. It must run on a
// SequencedTaskRunner that is properly configured for blocking I/O
// operations.
class CpuProbeLinux::BlockingTaskRunnerHelper final {
 public:
  explicit BlockingTaskRunnerHelper(base::FilePath procfs_stat_path);
  ~BlockingTaskRunnerHelper();

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  std::optional<CpuSample> Update();

 private:
  // Called when a core is seen the first time in /proc/stat.
  //
  // For most systems, the cores listed in /proc/stat are static. However, it
  // is theoretically possible for cores to go online and offline.
  void InitializeCore(size_t, const CoreTimes&);

  SEQUENCE_CHECKER(sequence_checker_);

  // /proc/stat parser. Used to derive CPU utilization.
  ProcfsStatCpuParser stat_parser_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Most recent per-core times from /proc/stat.
  std::vector<CoreTimes> last_per_core_times_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

CpuProbeLinux::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::FilePath procfs_stat_path)
    : stat_parser_(std::move(procfs_stat_path)) {}

CpuProbeLinux::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<CpuSample> CpuProbeLinux::BlockingTaskRunnerHelper::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stat_parser_.Update()) {
    return std::nullopt;
  }

  const std::vector<CoreTimes>& per_core_times = stat_parser_.core_times();

  double utilization_sum = 0.0;
  int utilization_cores = 0;
  for (size_t i = 0; i < per_core_times.size(); ++i) {
    CHECK_GE(last_per_core_times_.size(), i);

    const CoreTimes& core_times = per_core_times[i];

    if (last_per_core_times_.size() == i) {
      InitializeCore(i, core_times);
      continue;
    }

    double core_utilization =
        core_times.TimeUtilization(last_per_core_times_[i]);
    if (core_utilization >= 0) {
      // Only overwrite `last_per_core_times_` if the /proc/stat counters are
      // monotonically increasing. Otherwise, discard the measurement.
      last_per_core_times_[i] = core_times;

      utilization_sum += core_utilization;
      ++utilization_cores;
    }
  }

  if (utilization_cores > 0) {
    return CpuSample{.cpu_utilization = utilization_sum / utilization_cores};
  } else {
    return std::nullopt;
  }
}

void CpuProbeLinux::BlockingTaskRunnerHelper::InitializeCore(
    size_t core_index,
    const CoreTimes& initial_core_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(last_per_core_times_.size(), core_index);

  last_per_core_times_.push_back(initial_core_times);
}

// static
std::unique_ptr<CpuProbeLinux> CpuProbeLinux::Create() {
  return base::WrapUnique(
      new CpuProbeLinux(base::FilePath(ProcfsStatCpuParser::kProcfsStatPath)));
}

CpuProbeLinux::CpuProbeLinux(base::FilePath procfs_stat_path) {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      std::move(procfs_stat_path));
}

CpuProbeLinux::~CpuProbeLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeLinux::Update(SampleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_.AsyncCall(&BlockingTaskRunnerHelper::Update)
      .Then(std::move(callback));
}

base::WeakPtr<CpuProbe> CpuProbeLinux::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace system_cpu
