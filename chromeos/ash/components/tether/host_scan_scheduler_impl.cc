// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_scheduler_impl.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/session_manager/core/session_manager.h"

namespace ash::tether {

namespace {

// The InstantTethering.HostScanBatchDuration metric meaures the duration of a
// batch of host scans. A "batch" of host scans here refers to a group of host
// scans which occur just after each other. For example, two 30-second scans
// with a 5-second gap between them are considered a single 65-second batch host
// scan for this metric. For two back-to-back scans to be considered part of the
// same batch metric, they must be at most kMaxNumSecondsBetweenBatchScans
// seconds apart.
const int64_t kMaxNumSecondsBetweenBatchScans = 60;

// Minimum value for the scan length metric.
const int64_t kMinScanMetricSeconds = 1;

// Maximum value for the scan length metric.
const int64_t kMaxScanMetricsDays = 1;

// Number of buckets in the metric.
const int kNumMetricsBuckets = 1000;

}  // namespace

HostScanSchedulerImpl::HostScanSchedulerImpl(
    NetworkStateHandler* network_state_handler,
    HostScanner* host_scanner,
    session_manager::SessionManager* session_manager)
    : network_state_handler_(network_state_handler),
      host_scanner_(host_scanner),
      session_manager_(session_manager),
      host_scan_batch_timer_(std::make_unique<base::OneShotTimer>()),
      clock_(base::DefaultClock::GetInstance()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      is_screen_locked_(session_manager_->IsScreenLocked()) {
  network_state_handler_observer_.Observe(network_state_handler_.get());
  host_scanner_->AddObserver(this);
  session_manager_->AddObserver(this);
}

HostScanSchedulerImpl::~HostScanSchedulerImpl() {
  network_state_handler_->SetTetherScanState(false);
  host_scanner_->RemoveObserver(this);
  session_manager_->RemoveObserver(this);

  // If the most recent batch of host scans has already been logged, return
  // early.
  if (!host_scanner_->IsScanActive() && !host_scan_batch_timer_->IsRunning())
    return;

  // If a scan is still active during shutdown, there is not enough time to wait
  // for the scan to finish before logging its full duration. Instead, mark the
  // current time as the end of the scan so that it can be logged.
  if (host_scanner_->IsScanActive())
    last_scan_end_timestamp_ = clock_->Now();

  LogHostScanBatchMetric();
}

void HostScanSchedulerImpl::AttemptScanIfOffline() {
  const NetworkTypePattern network_type_pattern =
      switches::ShouldTetherHostScansIgnoreWiredConnections()
          ? NetworkTypePattern::Wireless()
          : NetworkTypePattern::Default();
  const NetworkState* first_network =
      network_state_handler_->FirstNetworkByType(network_type_pattern);
  if (IsOnlineOrHasActiveTetherConnection(first_network)) {
    PA_LOG(VERBOSE) << "Skipping scan attempt because the device is already "
                       "connected to a network.";
    return;
  }

  AttemptScan();
}

void HostScanSchedulerImpl::DefaultNetworkChanged(const NetworkState* network) {
  // If there is an active (i.e., connecting or connected) network, there is
  // no need to schedule a scan.
  if (IsOnlineOrHasActiveTetherConnection(network)) {
    return;
  }

  // Schedule a scan as part of a new task. Posting a task here ensures that
  // processing the default network change is done after other
  // NetworkStateHandlerObservers are finished running. Processing the
  // network change immediately can cause crashes; see
  // https://crbug.com/800370.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&HostScanSchedulerImpl::AttemptScan,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void HostScanSchedulerImpl::ScanRequested(const NetworkTypePattern& type) {
  if (NetworkTypePattern::Tether().MatchesPattern(type))
    AttemptScan();
}

void HostScanSchedulerImpl::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void HostScanSchedulerImpl::ScanFinished() {
  network_state_handler_->SetTetherScanState(false);

  last_scan_end_timestamp_ = clock_->Now();
  host_scan_batch_timer_->Start(
      FROM_HERE, base::Seconds(kMaxNumSecondsBetweenBatchScans),
      base::BindOnce(&HostScanSchedulerImpl::LogHostScanBatchMetric,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HostScanSchedulerImpl::OnSessionStateChanged() {
  TRACE_EVENT0("login", "HostScanSchedulerImpl::OnSessionStateChanged");
  bool was_screen_locked = is_screen_locked_;
  is_screen_locked_ = session_manager_->IsScreenLocked();

  if (is_screen_locked_) {
    // If the screen is now locked, stop any ongoing scan.
    host_scanner_->StopScan();
    return;
  }

  if (!was_screen_locked)
    return;

  // If the device was just unlocked, start a scan if not already connected to
  // a network.
  AttemptScanIfOffline();
}

void HostScanSchedulerImpl::SetTestDoubles(
    std::unique_ptr<base::OneShotTimer> test_host_scan_batch_timer,
    base::Clock* test_clock,
    scoped_refptr<base::TaskRunner> test_task_runner) {
  host_scan_batch_timer_ = std::move(test_host_scan_batch_timer);
  clock_ = test_clock;
  task_runner_ = test_task_runner;
}

void HostScanSchedulerImpl::AttemptScan() {
  // If already scanning, there is nothing to do.
  if (host_scanner_->IsScanActive())
    return;

  // If the screen is locked, a host scan should not occur.
  if (session_manager_->IsScreenLocked()) {
    PA_LOG(VERBOSE) << "Skipping scan attempt because the screen is locked.";
    return;
  }

  // If the timer is running, this new scan is part of the same batch as the
  // previous scan, so the timer should be stopped (it will be restarted after
  // the new scan finishes). If the timer is not running, the new scan is part
  // of a new batch, so the start timestamp should be recorded.
  if (host_scan_batch_timer_->IsRunning())
    host_scan_batch_timer_->Stop();
  else
    last_scan_batch_start_timestamp_ = clock_->Now();

  host_scanner_->StartScan();
  network_state_handler_->SetTetherScanState(true);
}

bool HostScanSchedulerImpl::IsTetherNetworkConnectingOrConnected() {
  return network_state_handler_->ConnectingNetworkByType(
             NetworkTypePattern::Tether()) ||
         network_state_handler_->ConnectedNetworkByType(
             NetworkTypePattern::Tether());
}

bool HostScanSchedulerImpl::IsOnlineOrHasActiveTetherConnection(
    const NetworkState* default_network) {
  return (default_network && default_network->IsConnectingOrConnected()) ||
         IsTetherNetworkConnectingOrConnected();
}

void HostScanSchedulerImpl::LogHostScanBatchMetric() {
  DCHECK(!last_scan_batch_start_timestamp_.is_null());
  DCHECK(!last_scan_end_timestamp_.is_null());

  base::TimeDelta batch_duration =
      last_scan_end_timestamp_ - last_scan_batch_start_timestamp_;
  UMA_HISTOGRAM_CUSTOM_TIMES("InstantTethering.HostScanBatchDuration",
                             batch_duration,
                             base::Seconds(kMinScanMetricSeconds) /* min */,
                             base::Days(kMaxScanMetricsDays) /* max */,
                             kNumMetricsBuckets /* bucket_count */);

  PA_LOG(VERBOSE) << "Logging host scan batch duration. Duration was "
                  << batch_duration.InSeconds() << " seconds.";
}

}  // namespace ash::tether
