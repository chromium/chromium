// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scheduler_config/no_op_scheduler_configuration_manager.h"

#include <memory>

namespace ash {

// static
std::unique_ptr<SchedulerConfigurationManagerBase>
SchedulerConfigurationManagerBase::Create(
    DebugDaemonClient* debug_daemon_client,
    PrefService* local_state) {
  return std::make_unique<NoOpSchedulerConfigurationManager>();
}

NoOpSchedulerConfigurationManager::NoOpSchedulerConfigurationManager() =
    default;

NoOpSchedulerConfigurationManager::~NoOpSchedulerConfigurationManager() =
    default;

std::optional<std::pair<bool, size_t>>
NoOpSchedulerConfigurationManager::GetLastReply() const {
  return std::make_pair(false, 0u);
}

}  // namespace ash
