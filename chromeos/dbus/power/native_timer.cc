// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/native_timer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/unix_domain_socket.h"
#include "base/rand_util.h"
#include "base/task_runner_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace chromeos {

namespace {

// Value of |timer_id_| when it's not initialized.
const PowerManagerClient::TimerId kNotCreatedId = -1;

// Value of |timer_id_| when creation was attempted but failed.
const PowerManagerClient::TimerId kErrorId = -2;

}  // namespace

NativeTimer::NativeTimer(const std::string& tag)
    : timer_id_(kNotCreatedId), tag_(tag) {
  // Create a socket pair, one end will be sent to the power daemon the other
  // socket will be used to listen for the timer firing.
  base::ScopedFD powerd_fd;
  base::ScopedFD expiration_fd;
  base::CreateSocketPair(&powerd_fd, &expiration_fd);
  if (!powerd_fd.is_valid() || !expiration_fd.is_valid()) {
    LOG(ERROR) << "Invalid file descriptor";
    timer_id_ = kErrorId;
    return;
  }

  // Send create timer request to the power daemon.
  std::vector<std::pair<clockid_t, base::ScopedFD>> create_timers_request;
  create_timers_request.push_back(
      std::make_pair(CLOCK_BOOTTIME_ALARM, std::move(powerd_fd)));
  PowerManagerClient::Get()->CreateArcTimers(
      tag, std::move(create_timers_request),
      base::BindOnce(&NativeTimer::OnCreateTimer, weak_factory_.GetWeakPtr(),
                     std::move(expiration_fd)));
}

NativeTimer::~NativeTimer() {
  // Delete the timer if it was created.
  if (timer_id_ < 0) {
    return;
  }

  PowerManagerClient::Get()->DeleteArcTimers(tag_, base::DoNothing());
}

struct NativeTimer::StartTimerParams {
  StartTimerParams() = default;
  StartTimerParams(base::TimeTicks absolute_expiration_time,
                   base::OnceClosure timer_expiration_callback,
                   OnStartNativeTimerCallback result_callback)
      : absolute_expiration_time(absolute_expiration_time),
        timer_expiration_callback(std::move(timer_expiration_callback)),
        result_callback(std::move(result_callback)) {}
  StartTimerParams(StartTimerParams&&) = default;
  ~StartTimerParams() = default;

  base::TimeTicks absolute_expiration_time;
  base::OnceClosure timer_expiration_callback;
  OnStartNativeTimerCallback result_callback;

  DISALLOW_COPY_AND_ASSIGN(StartTimerParams);
};

void NativeTimer::Start(base::TimeTicks absolute_expiration_time,
                        base::OnceClosure timer_expiration_callback,
                        OnStartNativeTimerCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the timer creation didn't succeed then tell the caller immediately.
  if (timer_id_ == kErrorId) {
    std::move(result_callback).Run(false);
    return;
  }

  // If the timer creation is in flight then save the parameters for this
  // method. |OnCreateTimer| will issue the start call.
  if (timer_id_ == kNotCreatedId) {
    // In normal scenarios of two back to back |Start| calls, the first one is
    // returned true in it's result callback and is overridden by the second
    // |Start| call. In the case of two back to back in flight |Start| calls
    // follow the same semantics and return true to the first caller.
    if (in_flight_start_timer_params_) {
      std::move(in_flight_start_timer_params_->result_callback).Run(true);
    }

    in_flight_start_timer_params_ = std::make_unique<StartTimerParams>(
        absolute_expiration_time, std::move(timer_expiration_callback),
        std::move(result_callback));
    return;
  }

  // Start semantics guarantee that it will override any old timer set. Reset
  // state to ensure this.
  ResetState();
  DCHECK_GE(timer_id_, 0);
  PowerManagerClient::Get()->StartArcTimer(
      timer_id_, absolute_expiration_time,
      base::BindOnce(&NativeTimer::OnStartTimer, weak_factory_.GetWeakPtr(),
                     std::move(timer_expiration_callback),
                     std::move(result_callback)));
}

void NativeTimer::OnCreateTimer(
    base::ScopedFD expiration_fd,
    base::Optional<std::vector<int32_t>> timer_ids) {
  DCHECK(expiration_fd.is_valid());
  if (!timer_ids.has_value()) {
    LOG(ERROR) << "No timers returned";
    timer_id_ = kErrorId;
    ProcessAndResetInFlightStartParams(false);
    return;
  }

  // Only one timer is being created.
  std::vector<int32_t> result = timer_ids.value();
  if (result.size() != 1) {
    LOG(ERROR) << "powerd created " << result.size() << " timers instead of 1";
    timer_id_ = kErrorId;
    ProcessAndResetInFlightStartParams(false);
    return;
  }

  // If timer creation failed and a |Start| call is pending then notify its
  // result callback an error.
  if (result[0] < 0) {
    LOG(ERROR) << "Error timer ID " << result[0];
    timer_id_ = kErrorId;
    ProcessAndResetInFlightStartParams(false);
    return;
  }

  // If timer creation succeeded and a |Start| call is pending then use the
  // stored parameters to schedule a timer.
  timer_id_ = result[0];
  expiration_fd_ = std::move(expiration_fd);
  ProcessAndResetInFlightStartParams(true);
}

void NativeTimer::OnStartTimer(base::OnceClosure timer_expiration_callback,
                               OnStartNativeTimerCallback result_callback,
                               bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    LOG(ERROR) << "Starting timer ID " << timer_id_ << " failed";
    std::move(result_callback).Run(false);
    return;
  }

  // At this point the timer has started, watch for its expiration and tell the
  // client that the start operation succeeded.
  timer_expiration_callback_ = std::move(timer_expiration_callback);
  expiration_fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      expiration_fd_.get(), base::BindRepeating(&NativeTimer::OnExpiration,
                                                weak_factory_.GetWeakPtr()));
  std::move(result_callback).Run(true);
}

void NativeTimer::OnExpiration() {
  // Write to the |expiration_fd_| to indicate to the instance that the timer
  // has expired. The instance expects 8 bytes on the read end similar to what
  // happens on a timerfd expiration. The timerfd API expects this to be the
  // number of expirations, however, more than one expiration isn't tracked
  // currently. This can block in the unlikely scenario of multiple writes
  // happening but the instance not reading the data. When the send queue is
  // full (64Kb), a write attempt here will block.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(expiration_fd_.is_valid());
  uint64_t timer_data;
  std::vector<base::ScopedFD> fds;
  if (!base::UnixDomainSocket::RecvMsg(expiration_fd_.get(), &timer_data,
                                       sizeof(timer_data), &fds)) {
    PLOG(ERROR) << "Bad data in expiration fd";
  }

  // If this isn't done then this task will keep running forever. Hence, clean
  // state regardless of any error above.
  ResetState();
  std::move(timer_expiration_callback_).Run();
}

void NativeTimer::ResetState() {
  weak_factory_.InvalidateWeakPtrs();
  expiration_fd_watcher_.reset();
  in_flight_start_timer_params_.reset();
}

void NativeTimer::ProcessAndResetInFlightStartParams(bool result) {
  if (!in_flight_start_timer_params_) {
    return;
  }

  // Run the result callback if |result| is false. Else schedule a timer with
  // the parameters stored.
  if (!result) {
    DCHECK_LT(timer_id_, 0);
    std::move(in_flight_start_timer_params_->result_callback).Run(false);
    in_flight_start_timer_params_.reset();
    return;
  }

  // The |in_flight_start_timer_params_->result_callback| will be called in
  // |OnStartTimer|.
  PowerManagerClient::Get()->StartArcTimer(
      timer_id_, in_flight_start_timer_params_->absolute_expiration_time,
      base::BindOnce(
          &NativeTimer::OnStartTimer, weak_factory_.GetWeakPtr(),
          std::move(in_flight_start_timer_params_->timer_expiration_callback),
          std::move(in_flight_start_timer_params_->result_callback)));

  // This state has been processed and must be reset to indicate that.
  in_flight_start_timer_params_.reset();
}

}  // namespace chromeos
