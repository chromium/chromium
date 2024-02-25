// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_poller.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/get_installed_version.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"

namespace {

bool g_disabled_for_testing = false;

// Bits in a testing options bitfield.
constexpr uint32_t kSimulateUpgrade = 0x01;
constexpr uint32_t kSimulateCriticalUpdate = 0x02;

// Returns a bitfield of the testing options that are selected via command line
// switches, or 0 if no options are selected.
uint32_t GetTestingOptions() {
  uint32_t testing_options = 0;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  if (cmd_line.HasSwitch(switches::kSimulateUpgrade))
    testing_options |= kSimulateUpgrade;

  // Simulating a critical update is a superset of simulating an upgrade.
  if (cmd_line.HasSwitch(switches::kSimulateCriticalUpdate))
    testing_options |= (kSimulateUpgrade | kSimulateCriticalUpdate);

  return testing_options;
}

// A GetInstalledVersionCallback implementation used when a regular or a
// critical update is simulated via --simulate-upgrade or
// --simulate-critical-update.
void SimulateGetInstalledVersion(uint32_t testing_options,
                                 InstalledVersionCallback callback) {
  DCHECK_NE(0U, testing_options);

  std::vector<uint32_t> components = version_info::GetVersion().components();
  components[3] += 2;

  InstalledAndCriticalVersion result((base::Version(components)));

  if ((testing_options & kSimulateCriticalUpdate) != 0) {
    --components[3];
    result.critical_version.emplace(std::move(components));
  }

  std::move(callback).Run(std::move(result));
}

// Returns the callback to get the installed version. Use of any testing option
// on the process command line results in use of the simulation function.
InstalledVersionPoller::GetInstalledVersionCallback
GetGetInstalledVersionCallback() {
  const uint32_t testing_options = GetTestingOptions();
  return testing_options ? base::BindRepeating(&SimulateGetInstalledVersion,
                                               testing_options)
                         : base::BindRepeating(&GetInstalledVersion);
}

// Returns the polling interval specified by --check-for-update-interval, or
// the default interval.
base::TimeDelta GetPollingInterval() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  const std::string seconds_str =
      cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec);
  int seconds;
  if (!seconds_str.empty() && base::StringToInt(seconds_str, &seconds))
    return base::Seconds(seconds);
  return InstalledVersionPoller::kDefaultPollingInterval;
}

}  // namespace

// InstalledVersionPoller::ScopedDisableForTesting ----------------------------

InstalledVersionPoller::ScopedDisableForTesting::ScopedDisableForTesting() {
  g_disabled_for_testing = true;
}

InstalledVersionPoller::ScopedDisableForTesting::~ScopedDisableForTesting() {
  g_disabled_for_testing = false;
}

// InstalledVersionPoller ------------------------------------------------------

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InstalledVersionPoller::PollType {
  kStartup = 0,   // The initial poll created at startup.
  kMonitor = 1,   // A poll in response to notification from the monitor.
  kPeriodic = 2,  // The periodic poll.
  kMaxValue = kPeriodic
};

// static
const base::TimeDelta InstalledVersionPoller::kDefaultPollingInterval =
    base::Hours(2);

InstalledVersionPoller::InstalledVersionPoller(BuildState* build_state)
    : InstalledVersionPoller(build_state,
                             GetGetInstalledVersionCallback(),
                             InstalledVersionMonitor::Create(),
                             nullptr) {}

InstalledVersionPoller::InstalledVersionPoller(
    BuildState* build_state,
    GetInstalledVersionCallback get_installed_version,
    std::unique_ptr<InstalledVersionMonitor> monitor,
    const base::TickClock* tick_clock)
    : build_state_(build_state),
      get_installed_version_(std::move(get_installed_version)),
      timer_(tick_clock) {
  // Make the first check in the background without delay. Suppress this if
  // polling is disabled for testing. This prevents all polling from taking
  // place since the result of poll N kicks off poll N+1.
  if (!g_disabled_for_testing) {
    StartMonitor(std::move(monitor));
    Poll(PollType::kStartup);
  }
}

InstalledVersionPoller::~InstalledVersionPoller() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstalledVersionPoller::StartMonitor(
    std::unique_ptr<InstalledVersionMonitor> monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!monitor_);
  monitor_ = std::move(monitor);
  monitor_->Start(base::BindRepeating(&InstalledVersionPoller::OnMonitorResult,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void InstalledVersionPoller::OnMonitorResult(bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error) {
    // Wait ten seconds before polling for the new version in case the monitor
    // provides multiple notifications during a normal update. Repeat
    // notifications will push back the poll.
    timer_.Start(FROM_HERE, base::Seconds(10),
                 base::BindOnce(&InstalledVersionPoller::Poll,
                                base::Unretained(this), PollType::kMonitor));
  } else {
    // An error occurred while monitoring; disable the monitor.
    monitor_.reset();
  }
}

void InstalledVersionPoller::Poll(PollType poll_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the result back via a weak pointer so that the result is dropped on the
  // floor should this instance be destroyed while polling.
  get_installed_version_.Run(
      base::BindOnce(&InstalledVersionPoller::OnInstalledVersion,
                     weak_ptr_factory_.GetWeakPtr(), poll_type));
}

void InstalledVersionPoller::OnInstalledVersion(
    PollType poll_type,
    InstalledAndCriticalVersion versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Consider either an invalid version or a higher version a normal update.
  BuildState::UpdateType update_type = BuildState::UpdateType::kNormalUpdate;
  if (versions.installed_version.IsValid()) {
    switch (versions.installed_version.CompareTo(version_info::GetVersion())) {
      case -1:
        update_type = BuildState::UpdateType::kEnterpriseRollback;
        break;
      case 0:
        update_type = BuildState::UpdateType::kNone;
        break;
    }
  }

  if (update_type == BuildState::UpdateType::kNone) {
    // The discovered version matches the current version, so report that no
    // update is available.
    build_state_->SetUpdate(update_type, base::Version(), std::nullopt);
  } else {
    // Either the installed version could not be discovered (invalid installed
    // version) or differs from the running version. Report it accordingly.
    build_state_->SetUpdate(update_type, versions.installed_version,
                            versions.critical_version);
  }

  // Gather statistics on population that could update but hasn't.
  UMA_HISTOGRAM_ENUMERATION("Chrome.BuildState.BuildStateUpdateType",
                            update_type);

  // Poll again after the polling interval passes.
  timer_.Start(FROM_HERE, GetPollingInterval(),
               base::BindOnce(&InstalledVersionPoller::Poll,
                              base::Unretained(this), PollType::kPeriodic));
}
