// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "components/memory_pressure/system_memory_pressure_evaluator_fuchsia.h"
#elif BUILDFLAG(IS_APPLE)
#include "components/memory_pressure/disk_space_memory_pressure_evaluator_mac.h"
#include "components/memory_pressure/system_memory_pressure_evaluator_mac.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
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
  // The native OS evaluator is always constructed so that the OS pressure
  // signal keeps being observed and reported in every experiment arm. The
  // feature flags control whether it votes, not whether it exists. When an
  // evaluator replaces it, it runs as a shadow evaluator with no voter.
  bool os_signal_replaced = false;
#if BUILDFLAG(IS_MAC)
  os_signal_replaced = base::FeatureList::IsEnabled(
      memory_pressure::features::kMacAvailableMemoryPressureEvaluator);
#endif
  return std::make_unique<memory_pressure::mac::SystemMemoryPressureEvaluator>(
      os_signal_replaced ? nullptr : monitor->CreateVoter());
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
          monitor->CreateVoter(/*is_disk_voter=*/true));
  disk_space_evaluator->Start();
  evaluators.push_back(std::move(disk_space_evaluator));
#endif

#if BUILDFLAG(IS_MAC)
  // Chrome-computed evaluators that can replace the native OS signal. Each is
  // an independent auxiliary voter.
  if (base::FeatureList::IsEnabled(
          memory_pressure::features::kMacAvailableMemoryPressureEvaluator)) {
    evaluators.push_back(
        ThresholdMemoryPressureEvaluator::CreateWithMacFeatureParams(
            monitor->CreateVoter()));
  }
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
  // Shadow evaluators are constructed without a voter. They observe and report
  // the system signal without influencing the aggregated pressure level.
  if (!voter_) {
    return;
  }
  voter_->SetVote(current_vote_);
}

void SystemMemoryPressureEvaluator::SendCurrentVote(bool notify) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Shadow evaluators are constructed without a voter. They observe and report
  // the system signal without influencing the aggregated pressure level.
  if (!voter_) {
    return;
  }
  voter_->SetVote(current_vote_, notify);
}

}  // namespace memory_pressure
