// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace ash {
namespace memory {

////////////////////////////////////////////////////////////////////////////////
// SystemMemoryPressureEvaluator
//
// A class to handle the observation of our free memory. It notifies the
// MemoryPressureListener of memory fill level changes, so that it can take
// action to reduce memory resources accordingly.
class COMPONENT_EXPORT(ASH_MEMORY) SystemMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator,
      public ResourcedClient::Observer {
 public:
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);
  ~SystemMemoryPressureEvaluator() override;

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

  // Returns the current system memory pressure evaluator.
  static SystemMemoryPressureEvaluator* Get();

  // Returns the cached amount of memory to reclaim.
  uint64_t GetCachedReclaimTargetKB();

 protected:
  // This constructor is only used for testing.
  SystemMemoryPressureEvaluator(
      bool for_testing,
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);

  // Implements ResourcedClient::Observer, protected for testing.
  void OnMemoryPressure(ResourcedClient::PressureLevel level,
                        uint64_t reclaim_target_kb) override;

 private:
  // Member variables.

  std::atomic<uint64_t> cached_reclaim_target_kb_{0};

  // We keep track of how long it has been since we last notified at the
  // moderate level.
  base::TimeTicks last_moderate_notification_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMemoryPressureEvaluator> weak_ptr_factory_;
};

}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
