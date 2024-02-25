// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_TIME_SYNC_TRACKER_FUCHSIA_H_
#define CHROMECAST_NET_TIME_SYNC_TRACKER_FUCHSIA_H_

#include <lib/zx/clock.h>

#include "base/message_loop/message_pump_for_io.h"
#include "chromecast/net/time_sync_tracker.h"

namespace chromecast {

class TimeSyncTrackerFuchsia : public TimeSyncTracker,
                               public base::MessagePumpForIO::ZxHandleWatcher {
 public:
  explicit TimeSyncTrackerFuchsia();
  TimeSyncTrackerFuchsia(const TimeSyncTrackerFuchsia&) = delete;
  TimeSyncTrackerFuchsia& operator=(const TimeSyncTrackerFuchsia&) = delete;
  ~TimeSyncTrackerFuchsia() override;

  // TimeSyncTracker implementation:
  bool IsTimeSynced() const final;

 private:
  // ZxHandleWatcher implementation:
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) final;

  zx::unowned_clock utc_clock_;
  bool is_time_synced_ = false;

  base::MessagePumpForIO::ZxHandleWatchController time_watch_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_TIME_SYNC_TRACKER_FUCHSIA_H_
