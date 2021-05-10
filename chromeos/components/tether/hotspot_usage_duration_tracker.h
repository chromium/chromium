// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOTSPOT_USAGE_DURATION_TRACKER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOTSPOT_USAGE_DURATION_TRACKER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/components/tether/active_host.h"

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

namespace tether {

// Records a metric which tracks the amount of time that users spend connected
// to Tether hotspots.
class HotspotUsageDurationTracker : public ActiveHost::Observer {
 public:
  explicit HotspotUsageDurationTracker(ActiveHost* active_host,
                                       base::Clock* clock);
  virtual ~HotspotUsageDurationTracker();

 protected:
  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  void HandleUnexpectedCurrentSession(
      const ActiveHost::ActiveHostStatus& active_host_status);

  ActiveHost* active_host_;
  base::Clock* clock_;

  base::Time last_connection_start_;

  DISALLOW_COPY_AND_ASSIGN(HotspotUsageDurationTracker);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOTSPOT_USAGE_DURATION_TRACKER_H_
