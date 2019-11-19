// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/power_manager_provider_impl.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace chromeos {
namespace assistant {

namespace {

// Tag used to identify Assistant timers.
constexpr char kTag[] = "Assistant";

// Used with wake lock APIs.
constexpr char kWakeLockReason[] = "Assistant";

// Copied from Chrome's //base/time/time_now_posix.cc.
// Returns count of |clk_id| in the form of a time delta. Returns an empty time
// delta if |clk_id| isn't present on the system.
base::TimeDelta ClockNow(clockid_t clk_id) {
  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    NOTREACHED() << "clock_gettime(" << clk_id << ") failed.";
    return base::TimeDelta();
  }
  return base::TimeDelta::FromTimeSpec(ts);
}

}  // namespace

PowerManagerProviderImpl::PowerManagerProviderImpl(
    mojom::Client* client,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
    : client_(client),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      weak_factory_(this) {
  DCHECK(client_);
}

PowerManagerProviderImpl::~PowerManagerProviderImpl() = default;

PowerManagerProviderImpl::AlarmId PowerManagerProviderImpl::AddWakeAlarm(
    uint64_t relative_time_ms,
    uint64_t max_delay_ms,
    assistant_client::Callback0 callback) {
  const AlarmId id = next_id_++;
  DVLOG(1) << __func__ << "Add alarm ID " << id << " for "
           << base::Time::Now() +
                  base::TimeDelta::FromMilliseconds(relative_time_ms);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerProviderImpl::AddWakeAlarmOnMainThread,
                     weak_factory_.GetWeakPtr(), id,
                     GetCurrentBootTime() +
                         base::TimeDelta::FromMilliseconds(relative_time_ms),
                     std::move(callback)));
  return id;
}

void PowerManagerProviderImpl::ExpireWakeAlarmNow(AlarmId id) {
  DVLOG(1) << __func__;
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerProviderImpl::OnTimerFiredOnMainThread,
                     weak_factory_.GetWeakPtr(), id));
}

void PowerManagerProviderImpl::AcquireWakeLock() {
  DVLOG(1) << __func__;
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerProviderImpl::AcquireWakeLockOnMainThread,
                     weak_factory_.GetWeakPtr()));
}

void PowerManagerProviderImpl::ReleaseWakeLock() {
  DVLOG(1) << __func__;
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerManagerProviderImpl::ReleaseWakeLockOnMainThread,
                     weak_factory_.GetWeakPtr()));
}

base::TimeTicks PowerManagerProviderImpl::GetCurrentBootTime() {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return base::TimeTicks() + ClockNow(CLOCK_BOOTTIME);
}

void PowerManagerProviderImpl::AddWakeAlarmOnMainThread(
    AlarmId id,
    base::TimeTicks absolute_expiration_time,
    assistant_client::Callback0 callback) {
  DVLOG(1) << __func__;
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  auto timer = std::make_unique<NativeTimer>(kTag + base::NumberToString(id));
  // Once the timer is created successfully, start the timer and store
  // associated data. The stored |callback| will be called in
  // |OnTimerFiredOnMainThread|.
  DVLOG(1) << "Starting timer with ID " << id << " for "
           << absolute_expiration_time;
  timer->Start(
      absolute_expiration_time,
      base::BindOnce(&PowerManagerProviderImpl::OnTimerFiredOnMainThread,
                     weak_factory_.GetWeakPtr(), id),
      base::BindOnce(&PowerManagerProviderImpl::OnStartTimerCallback,
                     weak_factory_.GetWeakPtr(), id));
  timers_[id] = std::make_pair(std::move(callback), std::move(timer));
}

void PowerManagerProviderImpl::AcquireWakeLockOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GE(wake_lock_count_, 0);

  wake_lock_count_++;
  if (wake_lock_count_ > 1) {
    DVLOG(1) << "Wake lock acquire. Count: " << wake_lock_count_;
    return;
  }

  // Initialize |wake_lock_| if this is the first time we're using it. Assistant
  // can acquire a wake lock even when it has nothing to show on the display,
  // this shouldn't wake the display up. Hence, the wake lock acquired is of
  // type kPreventAppSuspension.
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> provider;
    client_->RequestWakeLockProvider(provider.BindNewPipeAndPassReceiver());
    provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventAppSuspension,
        device::mojom::WakeLockReason::kOther, kWakeLockReason,
        wake_lock_.BindNewPipeAndPassReceiver());
  }

  DVLOG(1) << "Wake lock new acquire";
  // This would violate |GetWakeLockWithoutContext|'s API contract.
  DCHECK(wake_lock_);
  wake_lock_->RequestWakeLock();
}

void PowerManagerProviderImpl::ReleaseWakeLockOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GE(wake_lock_count_, 0);

  if (wake_lock_count_ == 0) {
    LOG(WARNING) << "Release without acquire. Count: " << wake_lock_count_;
    return;
  }

  wake_lock_count_--;
  if (wake_lock_count_ >= 1) {
    DVLOG(1) << "Wake lock release. Count: " << wake_lock_count_;
    return;
  }

  DCHECK(wake_lock_);
  DVLOG(1) << "Wake lock force release";
  wake_lock_->CancelWakeLock();
}

void PowerManagerProviderImpl::OnStartTimerCallback(AlarmId id, bool result) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!result) {
    // TODO(crbug.com/919984): Notify Assistant of error so that it can do
    // something meaningful in the UI.
    LOG(ERROR) << "Failed to start timer on alarm ID " << id;
    // Remove any metadata and resources associated with timers mapped to |id|.
    DCHECK_GT(timers_.erase(id), 0UL);
  }
}

void PowerManagerProviderImpl::OnTimerFiredOnMainThread(AlarmId id) {
  DVLOG(1) << __func__ << " ID " << id;
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  auto it = timers_.find(id);
  if (it == timers_.end()) {
    LOG(ERROR) << "Alarm with id " << id << " not found";
    return;
  }

  // Stop tracking the timer once it has fired.
  CallbackAndTimer& callback_and_timer = it->second;
  callback_and_timer.first();
  timers_.erase(id);
}

}  // namespace assistant
}  // namespace chromeos
