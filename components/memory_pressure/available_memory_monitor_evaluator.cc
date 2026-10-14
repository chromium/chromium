// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/available_memory_monitor_evaluator.h"

#include <utility>

#include "components/memory_pressure/memory_pressure_voter.h"

namespace memory_pressure {

AvailableMemoryMonitorEvaluator::AvailableMemoryMonitorEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : SystemMemoryPressureEvaluator(std::move(voter)) {
  StartObserving();
}

AvailableMemoryMonitorEvaluator::~AvailableMemoryMonitorEvaluator() {
  StopObserving();
}

void AvailableMemoryMonitorEvaluator::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AvailableMemoryMonitor::Get()->AddObserver(this);
}

void AvailableMemoryMonitorEvaluator::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AvailableMemoryMonitor::Get()->RemoveObserver(this);
}

void AvailableMemoryMonitorEvaluator::OnAvailableMemoryUpdated(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckMemoryPressure(sample);
}

void AvailableMemoryMonitorEvaluator::CheckMemoryPressure(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetCurrentVote(CalculateCurrentPressureLevel(sample));
  SendCurrentVote();
}

}  // namespace memory_pressure
