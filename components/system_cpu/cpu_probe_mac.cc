// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_cpu/cpu_probe_mac.h"

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/system_cpu/core_times.h"
#include "components/system_cpu/cpu_sample.h"
#include "components/system_cpu/host_processor_info_scanner.h"

namespace system_cpu {

// Helper class that performs the actual I/O. It must run on a
// SequencedTaskRunner that is properly configured for blocking I/O
// operations.
class CpuProbeMac::BlockingTaskRunnerHelper final {
 public:
  BlockingTaskRunnerHelper();
  ~BlockingTaskRunnerHelper();

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  std::optional<CpuSample> Update();

 private:
  // Called when a core is seen the first time.
  void InitializeCore(size_t, const CoreTimes&);

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to derive CPU utilization.
  HostProcessorInfoScanner processor_info_scanner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Most recent per-core times.
  std::vector<CoreTimes> last_per_core_times_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

CpuProbeMac::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper() = default;

CpuProbeMac::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<CpuSample> CpuProbeMac::BlockingTaskRunnerHelper::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  processor_info_scanner_.Update();
  const std::vector<CoreTimes>& per_core_times =
      processor_info_scanner_.core_times();

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
      // Only overwrite `last_per_core_times_` if the cpu time counters are
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

void CpuProbeMac::BlockingTaskRunnerHelper::InitializeCore(
    size_t core_index,
    const CoreTimes& initial_core_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(last_per_core_times_.size(), core_index);

  last_per_core_times_.push_back(initial_core_times);
}

// static
std::unique_ptr<CpuProbeMac> CpuProbeMac::Create() {
  return base::WrapUnique(new CpuProbeMac());
}

CpuProbeMac::CpuProbeMac() {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}

CpuProbeMac::~CpuProbeMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeMac::Update(SampleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_.AsyncCall(&BlockingTaskRunnerHelper::Update)
      .Then(std::move(callback));
}

base::WeakPtr<CpuProbe> CpuProbeMac::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace system_cpu
