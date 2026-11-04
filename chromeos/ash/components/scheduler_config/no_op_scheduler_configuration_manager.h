// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCHEDULER_CONFIG_NO_OP_SCHEDULER_CONFIGURATION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_SCHEDULER_CONFIG_NO_OP_SCHEDULER_CONFIGURATION_MANAGER_H_

#include <stddef.h>

#include <optional>
#include <utility>

#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"

namespace ash {

// A no-op implementation of SchedulerConfigurationManagerBase for non-x86_64
// architectures where Hyper-Threading / SMT scheduler configuration via debugd
// is not supported.
class NoOpSchedulerConfigurationManager
    : public SchedulerConfigurationManagerBase {
 public:
  NoOpSchedulerConfigurationManager();

  NoOpSchedulerConfigurationManager(const NoOpSchedulerConfigurationManager&) =
      delete;
  NoOpSchedulerConfigurationManager& operator=(
      const NoOpSchedulerConfigurationManager&) = delete;

  ~NoOpSchedulerConfigurationManager() override;

  // SchedulerConfigurationManagerBase overrides:
  std::optional<std::pair<bool, size_t>> GetLastReply() const override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SCHEDULER_CONFIG_NO_OP_SCHEDULER_CONFIGURATION_MANAGER_H_
