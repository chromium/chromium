// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace ash {
namespace memory {

namespace {
// Pointer to the SystemMemoryPressureEvaluator used by TabManagerDelegate for
// chromeos to need to call into ScheduleEarlyCheck.
SystemMemoryPressureEvaluator* g_system_evaluator = nullptr;

// We try not to re-notify on moderate too frequently, this time
// controls how frequently we will notify after our first notification.
constexpr base::TimeDelta kModerateMemoryPressureCooldownTime =
    base::Seconds(10);

}  // namespace

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
    : SystemMemoryPressureEvaluator(
          /*for_testing*/ false,
          std::move(voter)) {}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    bool for_testing,
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      weak_ptr_factory_(this) {
  DCHECK(g_system_evaluator == nullptr);
  g_system_evaluator = this;

  ResourcedClient* client = ResourcedClient::Get();
  if (client) {
    client->AddObserver(this);
  }
}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  DCHECK(g_system_evaluator);
  ResourcedClient* client = ResourcedClient::Get();
  if (client) {
    client->RemoveObserver(this);
  }
  g_system_evaluator = nullptr;
}

// static
SystemMemoryPressureEvaluator* SystemMemoryPressureEvaluator::Get() {
  return g_system_evaluator;
}

memory_pressure::ReclaimTarget
SystemMemoryPressureEvaluator::GetCachedReclaimTarget() {
  base::AutoLock lock(reclaim_target_lock_);
  return cached_reclaim_target_;
}

void SystemMemoryPressureEvaluator::OnMemoryPressure(
    ResourcedClient::PressureLevel level,
    memory_pressure::ReclaimTarget target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::MemoryPressureListener::MemoryPressureLevel listener_level;
  memory_pressure::ReclaimTarget new_reclaim_target;

  if (level == ResourcedClient::PressureLevel::CRITICAL) {
    listener_level =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

    // Only update the received target if pressure level is critical.
    new_reclaim_target = target;
  } else if (level == ResourcedClient::PressureLevel::MODERATE) {
    listener_level =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else {
    listener_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  // Store the new reclaim target.
  {
    base::AutoLock lock(reclaim_target_lock_);
    cached_reclaim_target_ = new_reclaim_target;
  }

  auto old_vote = current_vote();

  SetCurrentVote(listener_level);
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

  SendCurrentVote(notify);
}

}  // namespace memory
}  // namespace ash
