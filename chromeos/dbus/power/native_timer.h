// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_NATIVE_TIMER_H_
#define CHROMEOS_DBUS_POWER_NATIVE_TIMER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace chromeos {

using OnStartNativeTimerCallback = base::OnceCallback<void(bool)>;

// Sets timers that can also wake up the device from suspend by making D-Bus
// calls to the power daemon.
class COMPONENT_EXPORT(DBUS_POWER) NativeTimer {
 public:
  // While exists, `NativeTimer::Start` will fail and call `result_callback`
  // with failed status.
  class ScopedFailureSimulatorForTesting {
   public:
    ScopedFailureSimulatorForTesting();
    ~ScopedFailureSimulatorForTesting();
  };

  explicit NativeTimer(const std::string& tag);

  NativeTimer(const NativeTimer&) = delete;
  NativeTimer& operator=(const NativeTimer&) = delete;

  ~NativeTimer();

  // Starts a timer to expire at |absolute_expiration_time|. Runs
  // |timer_expiration_callback| on timer expiration. Runs |result_callback|
  // with the result of the start operation. If starting the timer failed then
  // |timer_expiration_callback| will not be called.
  //
  // Consecutive |Start| calls override the previous |Start| call.
  void Start(base::TimeTicks absolute_expiration_time,
             base::OnceClosure timer_expiration_callback,
             OnStartNativeTimerCallback result_callback);

 private:
  struct StartTimerParams;

  // D-Bus callback for a create timer D-Bus call.
  void OnCreateTimer(base::ScopedFD expiration_fd,
                     std::optional<std::vector<int32_t>> timer_ids);

  // D-Bus callback for a start timer D-Bus call.
  void OnStartTimer(base::OnceClosure timer_expiration_callback,
                    OnStartNativeTimerCallback result_callback,
                    bool result);

  // Callback for timer expiration.
  void OnExpiration();

  // Resets the |expiration_fd_watcher_| and cancels any inflight callbacks.
  void ResetState();

  // Calls the result callback for a pending |Start| operation with false iff
  // |result| is false. Else, schedules a timer using the D-Bus API and calls
  // the result callback for a pending |Start| operation with true. Resets
  // |in_flight_start_timer_params_| in all cases.
  void ProcessAndResetInFlightStartParams(bool result);

  // Stores the parameters for |Start| when timer is not yet created i.e.
  // |timer_id_| is uninitialized. Since |Start| calls override each other at
  // any point only the latest |Start| call's parameters are stored in this.
  std::unique_ptr<StartTimerParams> in_flight_start_timer_params_;

  // Timer id returned by the power daemon, to be used as a handle for the timer
  // APIs.
  PowerManagerClient::TimerId timer_id_;

  // Tag associated with the D-Bus API. Cached for deleting the timer in the
  // destructor.
  std::string tag_;

  // File descriptor that will be written to when a Chrome OS alarm fires.
  base::ScopedFD expiration_fd_;

  // Callback to run when the timer expires.
  base::OnceClosure timer_expiration_callback_;

  // Watches |expiration_fd_| for an event.
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      expiration_fd_watcher_;

  // Indicating if the timer creation should fail. Only set by tests.
  static bool simulate_timer_creation_failure_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NativeTimer> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_NATIVE_TIMER_H_
