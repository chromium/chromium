// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_POWER_MANAGER_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_POWER_MANAGER_PROVIDER_IMPL_H_

#include <map>
#include <memory>
#include <utility>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/dbus/power/native_timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace ash::libassistant {

// Implementation of power management features for libassistant.
//
// This object lives on Assistant service's main thread i.e. on the same
// sequence as |main_thread_task_runner_|. It is safe to assume that no lifetime
// issues will occur if all its state is accessed from
// |main_thread_task_runner_|. Any tasks posted on that will be invalidated if
// this object dies.
//
// However, the public API of this class is called by libassistant and has no
// guarantees on which thread it's called on. Therefore each method posts a task
// on |main_thread_task_runner_| to mimic running the API on the same sequence
// for thread safe access to all class members, as well as using Chrome APIs in
// a thread safe manner.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) PowerManagerProviderImpl
    : public assistant_client::PowerManagerProvider {
 public:
  PowerManagerProviderImpl();

  PowerManagerProviderImpl(const PowerManagerProviderImpl&) = delete;
  PowerManagerProviderImpl& operator=(const PowerManagerProviderImpl&) = delete;

  ~PowerManagerProviderImpl() override;

  void Initialize(mojom::PlatformDelegate* delegate);

  // assistant_client::PowerManagerProvider overrides. These are called from
  // libassistant threads.
  AlarmId AddWakeAlarm(uint64_t relative_time_ms,
                       uint64_t max_delay_ms,
                       assistant_client::Callback0 callback) override;
  void ExpireWakeAlarmNow(AlarmId alarm_id) override;
  void AcquireWakeLock() override;
  void ReleaseWakeLock() override;

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    DCHECK(tick_clock);
    tick_clock_ = tick_clock;
  }

 private:
  using CallbackAndTimer = std::pair<assistant_client::Callback0,
                                     std::unique_ptr<chromeos::NativeTimer>>;

  // Returns time ticks from boot including time ticks during sleeping.
  base::TimeTicks GetCurrentBootTime();

  // Creates a native timer by calling |NativeTimer::Create|. Runs on
  // |main_thread_task_runner_|.
  void AddWakeAlarmOnMainThread(AlarmId id,
                                base::TimeTicks absolute_expiration_time,
                                assistant_client::Callback0 callback);

  // Creates (if not created) and acquires |wake_lock_|.
  // Runs on |main_thread_task_runner_|.
  void AcquireWakeLockOnMainThread();

  // Releases |wake_lock_|. Runs on |main_thread_task_runner_|.
  void ReleaseWakeLockOnMainThread();

  // Callback for a start operation on a |NativeTimer|.
  void OnStartTimerCallback(AlarmId id, bool result);

  // Callback for timer with id |id| added via |AddWakeAlarm|. It calls the
  // corresponding client callback associated with the alarm. Removes the entry
  // for |id| from |timers_|.
  void OnTimerFiredOnMainThread(AlarmId id);

  // Store of currently active alarm ids returned to clients and the
  // corresponding pair of timer objects and client callbacks.
  std::map<AlarmId, CallbackAndTimer> timers_;

  // Monotonically increasing id used as key in |timers_| and assigned to timers
  // in |AddWakeAlarm|. This is the only member that is accessed from
  // libassistant threads only and not from |main_thread_task_runner_|.
  AlarmId next_id_ = 1;

  // Lazily initialized in response to the first call to |AcquireWakeLock|.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  // Current number of clients that requested |wake_lock_|. On zero |wake_lock_|
  // is released.
  int wake_lock_count_ = 0;

  // Used to post tasks from a libassistant thread on to the main thread in
  // order to use Chrome APIs safely.
  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  // Owned by |PlatformApi|. Used to initialize |wake_lock_|.
  raw_ptr<mojom::PlatformDelegate> platform_delegate_ = nullptr;

  // Clock to use to calculate time ticks. Set and used only for testing.
  raw_ptr<const base::TickClock> tick_clock_ = nullptr;

  base::WeakPtrFactory<PowerManagerProviderImpl> weak_factory_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_POWER_MANAGER_PROVIDER_IMPL_H_
