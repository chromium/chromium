// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scheduler_config/scheduler_configuration_manager.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

#if !defined(ARCH_CPU_X86_64)
#error "SchedulerConfigurationManager is only supported on x86_64."
#endif

namespace ash {

namespace {

constexpr base::FeatureParam<std::string> kSchedulerConfigurationParam{
    &features::kSchedulerConfiguration, "config", ""};

}  // namespace

// static
std::unique_ptr<SchedulerConfigurationManagerBase>
SchedulerConfigurationManagerBase::Create(
    DebugDaemonClient* debug_daemon_client,
    PrefService* local_state) {
  return std::make_unique<SchedulerConfigurationManager>(debug_daemon_client,
                                                         local_state);
}

SchedulerConfigurationManager::SchedulerConfigurationManager(
    DebugDaemonClient* debug_daemon_client,
    PrefService* local_state)
    : debug_daemon_client_(debug_daemon_client) {
  observer_.Init(local_state);
  observer_.Add(
      prefs::kSchedulerConfiguration,
      base::BindRepeating(&SchedulerConfigurationManager::OnPrefChange,
                          base::Unretained(this)));
  // WaitForServiceToBeAvailable() has no built-in timeout, so log a warning if
  // debugd does not register on D-Bus promptly.
  debugd_wait_timer_.Start(
      FROM_HERE, kDebugdWaitWarningTimeout, this,
      &SchedulerConfigurationManager::OnDebugdAvailableTimeout);
  debug_daemon_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&SchedulerConfigurationManager::OnDebugDaemonReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

SchedulerConfigurationManager::~SchedulerConfigurationManager() = default;

std::optional<std::pair<bool, size_t>>
SchedulerConfigurationManager::GetLastReply() const {
  return last_reply_;
}

void SchedulerConfigurationManager::OnDebugDaemonReady(bool service_is_ready) {
  debugd_wait_timer_.Stop();
  if (!service_is_ready) {
    LOG(ERROR) << "Debug daemon unavailable";
    return;
  }

  // Initialize the system.
  debug_daemon_ready_ = true;
  OnPrefChange();
}

void SchedulerConfigurationManager::OnPrefChange() {
  // No point in calling debugd if it isn't ready yet. The ready callback will
  // will call this function again to set the initial configuration.
  if (!debug_daemon_ready_) {
    return;
  }

  // Determine the effective configuration name.
  std::string config_name;
  PrefService* local_state = observer_.prefs();
  std::string feature_param_value = kSchedulerConfigurationParam.Get();
  std::string cmdline_default =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSchedulerConfigurationDefault);

  // A user-set or policy-set configuration takes precedence.
  if (local_state->HasPrefPath(prefs::kSchedulerConfiguration)) {
    config_name = local_state->GetString(prefs::kSchedulerConfiguration);
  } else if (!feature_param_value.empty()) {
    // Next check for a Finch feature setting.
    config_name = feature_param_value;
  } else if (!cmdline_default.empty()) {
    // Next, if Finch isn't set, see if the command line passed in a default.
    config_name = cmdline_default;
  } else {
    // If nothing is found, default to core isolation scheduling. Note that
    // only some kernels support core isolation. debugd checks for support and
    // reverts to conservative if core isolation is unavailable.
    config_name = debugd::scheduler_configuration::kCoreIsolationScheduler;
  }

  // NB: Also send an update when the config gets reset to let the system pick
  // whatever default. Note that the value we read in this case will be the
  // default specified on pref registration, e.g. empty string.
  ++pending_configuration_requests_;
  debugd_wait_timer_.Start(
      FROM_HERE, kDebugdWaitWarningTimeout, this,
      &SchedulerConfigurationManager::OnConfigurationReplyTimeout);
  debug_daemon_client_->SetSchedulerConfigurationV2(
      config_name,
      /*lock_policy=*/false,
      base::BindOnce(&SchedulerConfigurationManager::OnConfigurationSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SchedulerConfigurationManager::OnConfigurationSet(
    bool result,
    size_t num_cores_disabled) {
  CHECK_GT(pending_configuration_requests_, 0u);
  --pending_configuration_requests_;
  if (pending_configuration_requests_ == 0) {
    debugd_wait_timer_.Stop();
  }
  last_reply_ = std::make_pair(result, num_cores_disabled);

  if (result) {
    VLOG(1) << num_cores_disabled << " logical CPU cores are disabled";
  } else {
    LOG(ERROR) << "Failed to update scheduler configuration";
  }
  for (Observer& obs : observer_list_)
    obs.OnConfigurationSet(result, num_cores_disabled);
}

void SchedulerConfigurationManager::OnDebugdAvailableTimeout() {
  CHECK(!debug_daemon_ready_);
  LOG(WARNING) << "Still waiting for debugd D-Bus service to become "
                  "available for scheduler configuration after "
               << kDebugdWaitWarningTimeout;
}

void SchedulerConfigurationManager::OnConfigurationReplyTimeout() {
  CHECK_GT(pending_configuration_requests_, 0u);
  LOG(WARNING) << "Still waiting for scheduler configuration reply from "
                  "debugd after "
               << kDebugdWaitWarningTimeout;
}

}  // namespace ash
