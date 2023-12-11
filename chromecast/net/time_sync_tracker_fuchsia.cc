// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/time_sync_tracker_fuchsia.h"

#include <lib/zx/clock.h>
#include <zircon/utc.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/current_thread.h"
#include "base/time/time.h"

namespace chromecast {
namespace {

zx_handle_t GetUtcClockHandle() {
  zx_handle_t clock_handle = zx_utc_reference_get();
  DCHECK(clock_handle != ZX_HANDLE_INVALID);
  return clock_handle;
}

}  // namespace

TimeSyncTrackerFuchsia::TimeSyncTrackerFuchsia()
    : utc_clock_(GetUtcClockHandle()), time_watch_(FROM_HERE) {
  base::CurrentIOThread::Get()->WatchZxHandle(
      utc_clock_->get(), false /* persistent */, ZX_USER_SIGNAL_0, &time_watch_,
      this);
}

TimeSyncTrackerFuchsia::~TimeSyncTrackerFuchsia() = default;

void TimeSyncTrackerFuchsia::OnZxHandleSignalled(zx_handle_t handle,
                                                 zx_signals_t signals) {
  VLOG(1) << " Time is externally synced.";
  is_time_synced_ = true;
  Notify();
}

bool TimeSyncTrackerFuchsia::IsTimeSynced() const {
  return is_time_synced_;
}

}  // namespace chromecast
