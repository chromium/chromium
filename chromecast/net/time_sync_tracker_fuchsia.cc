// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/time_sync_tracker_fuchsia.h"

#include <lib/async/default.h>
#include <lib/zx/clock.h>
#include <zircon/time.h>
#include <zircon/utc.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace chromecast {
namespace {

const unsigned int kTimeSyncRetrySeconds = 1;

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
  if (is_time_synced_)
    return;
  if (wait_for_utc_ && wait_for_utc_->is_pending())
    return;
  wait_for_utc_ = std::make_unique<async::WaitOnce>(
      utc_clock_->get(), ZX_USER_SIGNAL_0, /*options=*/0);
  auto wait_for_utc_status = wait_for_utc_->Begin(
      async_get_default_dispatcher(),
      [this](auto*, auto*, const zx_status_t status, ...) mutable {
        if (status == ZX_OK) {
          is_time_synced_ = true;
          Notify();
        } else {
          DVLOG(2) << "Syncing time with external clock failed with status,"
                   << "will try again. status failed with" << status;
          task_runner_->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&TimeSyncTrackerFuchsia::OnNetworkConnected,
                             weak_this_),
              base::TimeDelta::FromSeconds(kTimeSyncRetrySeconds));
        }
      });
  ZX_DCHECK(wait_for_utc_status == ZX_OK, wait_for_utc_status)
      << "zx_wait_for_utc_begin";
}

bool TimeSyncTrackerFuchsia::IsTimeSynced() const {
  return is_time_synced_;
}

}  // namespace chromecast
