// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H_

namespace ash {

namespace tether {

// Schedules scans for Tether hosts.
class HostScanScheduler {
 public:
  HostScanScheduler() {}

  HostScanScheduler(const HostScanScheduler&) = delete;
  HostScanScheduler& operator=(const HostScanScheduler&) = delete;

  virtual ~HostScanScheduler() {}

  // Attempts to perform a Tether host scan. If the device is already connected
  // to the internet, a scan will not be performed. If a scan is already active,
  // this function is a no-op.
  virtual void AttemptScanIfOffline() = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H_
