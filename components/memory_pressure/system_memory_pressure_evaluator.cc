// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if defined(OS_FUCHSIA)
#include "components/memory_pressure/system_memory_pressure_evaluator_fuchsia.h"
#elif defined(OS_MAC)
#include "components/memory_pressure/system_memory_pressure_evaluator_mac.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"
#endif

namespace memory_pressure {

#if defined(OS_WIN)
constexpr base::Feature kUseWinOSMemoryPressureSignals{
    "UseWinOSMemoryPressureSignals", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// static
std::unique_ptr<SystemMemoryPressureEvaluator>
SystemMemoryPressureEvaluator::CreateDefaultSystemEvaluator(
    MultiSourceMemoryPressureMonitor* monitor) {
#if defined(OS_FUCHSIA)
  return std::make_unique<
      memory_pressure::SystemMemoryPressureEvaluatorFuchsia>(
      monitor->CreateVoter());
#elif defined(OS_MAC)
  return std::make_unique<memory_pressure::mac::SystemMemoryPressureEvaluator>(
      monitor->CreateVoter());
#elif defined(OS_WIN)
  auto evaluator =
      std::make_unique<memory_pressure::win::SystemMemoryPressureEvaluator>(
          monitor->CreateVoter());
  // Also subscribe to the OS signals if they're available and the feature is
  // enabled.
  if (base::FeatureList::IsEnabled(kUseWinOSMemoryPressureSignals) &&
      base::win::GetVersion() >= base::win::Version::WIN8_1) {
    evaluator->CreateOSSignalPressureEvaluator(monitor->CreateVoter());
  }
  return evaluator;
#else
  // Chrome OS and Chromecast evaluators are created in separate components.
  return nullptr;
#endif
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : current_vote_(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      voter_(std::move(voter)) {}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() = default;

void SystemMemoryPressureEvaluator::SetCurrentVote(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_vote_ = level;
}

void SystemMemoryPressureEvaluator::SendCurrentVote(bool notify) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  voter_->SetVote(current_vote_, notify);
}

}  // namespace memory_pressure
