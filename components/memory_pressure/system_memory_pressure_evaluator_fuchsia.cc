// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_fuchsia.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "components/memory_pressure/memory_pressure_voter.h"

namespace memory_pressure {

namespace {

base::MemoryPressureLevel FuchsiaToBaseMemoryPressureLevel(
    fuchsia::memorypressure::Level level) {
  switch (level) {
    case fuchsia::memorypressure::Level::NORMAL:
      return base::MEMORY_PRESSURE_LEVEL_NONE;

    case fuchsia::memorypressure::Level::WARNING:
      return base::MEMORY_PRESSURE_LEVEL_MODERATE;

    case fuchsia::memorypressure::Level::CRITICAL:
      return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  };
}

}  // namespace

SystemMemoryPressureEvaluatorFuchsia::SystemMemoryPressureEvaluatorFuchsia(
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      binding_(this) {
  binding_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia.memorypressure.Provider"));

  DVLOG(1) << "Registering for memory pressure updates.";
  auto provider = base::ComponentContextForProcess()
                      ->svc()
                      ->Connect<fuchsia::memorypressure::Provider>();
  provider->RegisterWatcher(binding_.NewBinding());
}

SystemMemoryPressureEvaluatorFuchsia::~SystemMemoryPressureEvaluatorFuchsia() =
    default;

void SystemMemoryPressureEvaluatorFuchsia::OnLevelChanged(
    fuchsia::memorypressure::Level level,
    OnLevelChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "OnLevelChanged: level=" << static_cast<uint32_t>(level);

  base::MemoryPressureLevel new_level = FuchsiaToBaseMemoryPressureLevel(level);

  VLOG(1) << "MemoryPressureLevel: " << new_level;

  SetCurrentVote(new_level);
  SendCurrentVote();

  callback();
}

}  // namespace memory_pressure
