// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pressure/system_memory_pressure_evaluator.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace chromeos {
namespace memory {

namespace {
// Pointer to the SystemMemoryPressureEvaluator used by TabManagerDelegate for
// chromeos to need to call into ScheduleEarlyCheck.
SystemMemoryPressureEvaluator* g_system_evaluator = nullptr;

// We try not to re-notify on moderate too frequently, this time
// controls how frequently we will notify after our first notification.
constexpr base::TimeDelta kModerateMemoryPressureCooldownTime =
    base::TimeDelta::FromSeconds(10);

// Converts an available memory value in MB to a memory pressure level.
base::MemoryPressureListener::MemoryPressureLevel
GetMemoryPressureLevelFromAvailable(uint64_t available_kb,
                                    uint64_t moderate_margin_kb,
                                    uint64_t critical_margin_kb) {
  if (available_kb < critical_margin_kb)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  if (available_kb < moderate_margin_kb)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

}  // namespace

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : SystemMemoryPressureEvaluator(
          /*for_testing*/ false,
          std::move(voter)) {}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    bool for_testing,
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)),
      cached_available_kb_(0),
      weak_ptr_factory_(this) {
  DCHECK(g_system_evaluator == nullptr);
  g_system_evaluator = this;

  // Setting up default margins in case the D-Bus method call failed.
  SetupDefaultMemoryMargins();

  if (!for_testing) {
    chromeos::ResourcedClient* client = chromeos::ResourcedClient::Get();
    if (client) {
      client->GetMemoryMarginsKB(
          base::BindOnce(&SystemMemoryPressureEvaluator::OnMemoryMargins,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    // We will check the memory pressure and report the metric
    // (ChromeOS.MemoryPressureLevel) every 1 second.
    checking_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1),
        base::BindRepeating(&SystemMemoryPressureEvaluator::
                                CheckMemoryPressureAndRecordStatistics,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}
SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  DCHECK(g_system_evaluator);
  g_system_evaluator = nullptr;
}

// static
SystemMemoryPressureEvaluator* SystemMemoryPressureEvaluator::Get() {
  return g_system_evaluator;
}

// CheckMemoryPressure will get the current memory pressure level by checking
// the available memory.
void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  chromeos::ResourcedClient* client = chromeos::ResourcedClient::Get();
  if (client) {
    client->GetAvailableMemoryKB(
        base::BindOnce(&SystemMemoryPressureEvaluator::OnAvailableMemory,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SystemMemoryPressureEvaluator::CheckMemoryPressureImpl(
    uint64_t moderate_margin_kb,
    uint64_t critical_margin_kb,
    uint64_t mem_avail_kb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto old_vote = current_vote();

  SetCurrentVote(GetMemoryPressureLevelFromAvailable(
      mem_avail_kb, moderate_margin_kb, critical_margin_kb));
  bool notify = true;

  if (current_vote() ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    last_moderate_notification_ = base::TimeTicks();
    notify = false;
  } else if (current_vote() ==
             base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    // In the case of MODERATE memory pressure we may be in this state for quite
    // some time so we limit the rate at which we dispatch notifications.
    if (old_vote == current_vote()) {
      if (base::TimeTicks::Now() - last_moderate_notification_ <
          kModerateMemoryPressureCooldownTime) {
        notify = false;
      } else if (old_vote ==
                 base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
        // Reset the moderate notification time if we just crossed back.
        last_moderate_notification_ = base::TimeTicks::Now();
        notify = false;
      }
    }

    if (notify)
      last_moderate_notification_ = base::TimeTicks::Now();
  }

  VLOG(1) << "SystemMemoryPressureEvaluator::CheckMemoryPressure dispatching "
             "at level: "
          << current_vote();
  SendCurrentVote(notify);
}

void SystemMemoryPressureEvaluator::CheckMemoryPressureAndRecordStatistics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: If we support notifications of memory pressure changes in both
  // directions we will not have to update the cached value as it will always
  // be correct.
  CheckMemoryPressure();

  // Record UMA histogram statistics for the current memory pressure level, it
  // would seem that only Memory.PressureLevel would be necessary.
  constexpr int kNumberPressureLevels = 3;
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.MemoryPressureLevel", current_vote(),
                            kNumberPressureLevels);
}

void SystemMemoryPressureEvaluator::ScheduleEarlyCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemMemoryPressureEvaluator::SetupDefaultMemoryMargins() {
  base::SystemMemoryInfoKB info;
  uint64_t total_memory_kb = 2 * 1024 * 1024;
  if (base::GetSystemMemoryInfo(&info)) {
    total_memory_kb = static_cast<uint64_t>(info.total);
  } else {
    PLOG(ERROR)
        << "Assume 2 GiB total memory if opening/parsing meminfo failed";
    LOG_IF(FATAL, base::SysInfo::IsRunningOnChromeOS())
        << "procfs isn't mounted or unable to open /proc/meminfo";
  }

  // Critical margin is 5.2% of total memory, moderate margin is 40% of total
  // memory. See also /usr/share/cros/init/swap.sh on DUT.
  critical_margin_kb_ = total_memory_kb * 13 / 250;
  moderate_margin_kb_ = total_memory_kb * 2 / 5;
}

SystemMemoryPressureEvaluator::MemoryMarginsKB
SystemMemoryPressureEvaluator::GetMemoryMarginsKB() {
  return MemoryMarginsKB{.critical = critical_margin_kb_,
                         .moderate = moderate_margin_kb_};
}

uint64_t SystemMemoryPressureEvaluator::GetCachedAvailableMemoryKB() {
  return cached_available_kb_.load();
}

void SystemMemoryPressureEvaluator::OnMemoryMargins(
    absl::optional<chromeos::ResourcedClient::MemoryMarginsKB> result) {
  // The bus daemon never reorders messages. That is, if you send two method
  // call messages to the same recipient, they will be received in the order
  // they were sent [1].
  //
  // OnMemoryMargins is the callback to SystemMemoryPressureEvaluator's first
  // dbus call, and reading critical_margin_kb_ is on/after the
  // OnAvailableMemory dbus method callback. So it's safe to write to
  // critical_margin_kb_ and moderate_margin_kb_ without a mutex in
  // OnMemoryMargins.
  //
  // [1]: https://dbus.freedesktop.org/doc/dbus-tutorial.html#callprocedure
  if (result.has_value()) {
    critical_margin_kb_ = result.value().critical;
    moderate_margin_kb_ = result.value().moderate;
  } else {
    LOG(ERROR) << "Failed to get the memory margins with D-Bus";
  }
}

void SystemMemoryPressureEvaluator::OnAvailableMemory(
    absl::optional<uint64_t> result) {
  if (result.has_value()) {
    uint64_t mem_avail_kb = result.value();
    cached_available_kb_.store(mem_avail_kb);
    CheckMemoryPressureImpl(moderate_margin_kb_, critical_margin_kb_,
                            mem_avail_kb);
  } else {
    static bool error_printed = false;
    if (!error_printed) {
      LOG(ERROR) << "Failed to get available memory with D-Bus";
      error_printed = true;
    }
  }
}

}  // namespace memory
}  // namespace chromeos
