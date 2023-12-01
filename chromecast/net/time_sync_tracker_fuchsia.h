// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_TIME_SYNC_TRACKER_FUCHSIA_H_
#define CHROMECAST_NET_TIME_SYNC_TRACKER_FUCHSIA_H_

#include <lib/zx/clock.h>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/net/time_sync_tracker.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {

class TimeSyncTrackerFuchsia : public TimeSyncTracker {
 public:
  explicit TimeSyncTrackerFuchsia(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  TimeSyncTrackerFuchsia(const TimeSyncTrackerFuchsia&) = delete;
  TimeSyncTrackerFuchsia& operator=(const TimeSyncTrackerFuchsia&) = delete;
  ~TimeSyncTrackerFuchsia() override;

  // TimeSyncTracker implementation:
  void OnNetworkConnected() final;
  bool IsTimeSynced() const final;

 private:
  void Poll();

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  zx::unowned_clock utc_clock_;

  bool is_polling_ = false;
  bool is_time_synced_ = false;

  base::WeakPtr<TimeSyncTrackerFuchsia> weak_this_;
  base::WeakPtrFactory<TimeSyncTrackerFuchsia> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_TIME_SYNC_TRACKER_FUCHSIA_H_
