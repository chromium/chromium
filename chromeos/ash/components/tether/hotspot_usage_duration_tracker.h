// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOTSPOT_USAGE_DURATION_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOTSPOT_USAGE_DURATION_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/tether/active_host.h"

namespace base {
class Clock;
}  // namespace base

namespace ash {

namespace tether {

// Records a metric which tracks the amount of time that users spend connected
// to Tether hotspots.
class HotspotUsageDurationTracker : public ActiveHost::Observer {
 public:
  explicit HotspotUsageDurationTracker(ActiveHost* active_host,
                                       base::Clock* clock);

  HotspotUsageDurationTracker(const HotspotUsageDurationTracker&) = delete;
  HotspotUsageDurationTracker& operator=(const HotspotUsageDurationTracker&) =
      delete;

  virtual ~HotspotUsageDurationTracker();

 protected:
  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  void HandleUnexpectedCurrentSession(
      const ActiveHost::ActiveHostStatus& active_host_status);

  raw_ptr<ActiveHost> active_host_;
  raw_ptr<base::Clock> clock_;

  base::Time last_connection_start_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOTSPOT_USAGE_DURATION_TRACKER_H_
