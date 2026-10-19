// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator.h"

#include <memory>
#include <utility>

#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "components/memory_pressure/system_memory_pressure_evaluator_fuchsia.h"
#elif BUILDFLAG(IS_APPLE)
#include "components/memory_pressure/disk_space_memory_pressure_evaluator_mac.h"
#include "components/memory_pressure/system_memory_pressure_evaluator_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "components/memory_pressure/threshold_memory_pressure_evaluator.h"
#endif

namespace memory_pressure {

// static
std::unique_ptr<SystemMemoryPressureEvaluator>
SystemMemoryPressureEvaluator::CreateDefaultSystemEvaluator(
    MultiSourceMemoryPressureMonitor* monitor) {
#if BUILDFLAG(IS_FUCHSIA)
  return std::make_unique<
      memory_pressure::SystemMemoryPressureEvaluatorFuchsia>(
      monitor->CreateVoter());
#elif BUILDFLAG(IS_APPLE)
  return std::make_unique<memory_pressure::mac::SystemMemoryPressureEvaluator>(
      monitor->CreateVoter());
#elif BUILDFLAG(IS_WIN)
  return ThresholdMemoryPressureEvaluator::CreateWithWinDefaults(
      monitor->CreateVoter());
#else
  // Chrome OS and Chromecast evaluators are created in separate components.
  return nullptr;
#endif
}

// static
std::vector<std::unique_ptr<SystemMemoryPressureEvaluator>>
SystemMemoryPressureEvaluator::CreateDefaultAuxiliaryEvaluators(
    MultiSourceMemoryPressureMonitor* monitor) {
  std::vector<std::unique_ptr<SystemMemoryPressureEvaluator>> evaluators;
#if BUILDFLAG(IS_APPLE)
  auto disk_space_evaluator =
      std::make_unique<memory_pressure::mac::DiskSpaceMemoryPressureEvaluator>(
          monitor->CreateVoter());
  disk_space_evaluator->Start();
  evaluators.push_back(std::move(disk_space_evaluator));
#endif
  return evaluators;
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : current_vote_(base::MEMORY_PRESSURE_LEVEL_NONE),
      voter_(std::move(voter)) {}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() = default;

void SystemMemoryPressureEvaluator::SetCurrentVote(
    base::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_vote_ = level;
}

void SystemMemoryPressureEvaluator::SendCurrentVote() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  voter_->SetVote(current_vote_);
}

void SystemMemoryPressureEvaluator::SendCurrentVote(bool notify) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  voter_->SetVote(current_vote_, notify);
}

}  // namespace memory_pressure
