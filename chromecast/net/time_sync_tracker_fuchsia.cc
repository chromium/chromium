// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/time_sync_tracker_fuchsia.h"

#include <lib/zx/clock.h>
#include <zircon/utc.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace chromecast {
namespace {

// How often zx::clock is polled in seconds.
const unsigned int kPollPeriodSeconds = 1;

zx_handle_t GetUtcClockHandle() {
  zx_handle_t clock_handle = zx_utc_reference_get();
  DCHECK(clock_handle != ZX_HANDLE_INVALID);
  return clock_handle;
}

}  // namespace

TimeSyncTrackerFuchsia::TimeSyncTrackerFuchsia(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
  : task_runner_(std::move(task_runner)),
    utc_clock_(GetUtcClockHandle()),
    weak_factory_(this) {
  DCHECK(task_runner_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

TimeSyncTrackerFuchsia::~TimeSyncTrackerFuchsia() = default;

void TimeSyncTrackerFuchsia::OnNetworkConnected() {
  if (!is_polling_ && !is_time_synced_) {
    is_polling_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TimeSyncTrackerFuchsia::Poll, weak_this_));
  }
}

bool TimeSyncTrackerFuchsia::IsTimeSynced() const {
  return is_time_synced_;
}

void TimeSyncTrackerFuchsia::Poll() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(is_polling_);

  zx_clock_details_v1_t details;
  zx_status_t status = utc_clock_->get_details(&details);
  ZX_CHECK(status == ZX_OK, status) << "zx_clock_get_details";

  is_time_synced_ =
    details.backstop_time != details.ticks_to_synthetic.synthetic_offset;
  DVLOG(2) << __func__ << ": backstop_time=" << details.backstop_time
           << ", synthetic_offset=" << details.ticks_to_synthetic.synthetic_offset
           << ", synced=" << is_time_synced_;

  if (!is_time_synced_) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&TimeSyncTrackerFuchsia::Poll, weak_this_),
        base::Seconds(kPollPeriodSeconds));
    return;
  }

  is_polling_ = false;
  Notify();
}

}  // namespace chromecast
