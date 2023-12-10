// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_SCHEDULER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace ash::nearby {

// Schedules tasks and alerts the owner when a request is ready. Scheduling
// begins after Start() is called, and scheduling is stopped via Stop().
//
// An immediate request can be made, bypassing any current scheduling, via
// MakeImmediateRequest(). When an request attempt has completed--successfully
// or not--the owner should invoke HandleResult() so the scheduler can process
// the attempt outcomes and schedule future attempts if necessary.
class NearbyScheduler {
 public:
  using OnRequestCallback = base::RepeatingCallback<void()>;

  explicit NearbyScheduler(OnRequestCallback callback);
  virtual ~NearbyScheduler();

  void Start();
  void Stop();
  bool is_running() const { return is_running_; }

  // Makes a request that runs as soon as possible.
  virtual void MakeImmediateRequest() = 0;

  // Processes the result of the previous request. Method to be called by the
  // owner when the request is finished. The timer for the next request is
  // automatically scheduled.
  virtual void HandleResult(bool success) = 0;

  // Recomputes the time until the next request, using GetTimeUntilNextRequest()
  // as the source of truth. This method is essentially idempotent. NOTE: This
  // method should rarely need to be called.
  virtual void Reschedule() = 0;

  // Returns the time of the last known successful request. If no request has
  // succeeded, std::nullopt is returned.
  virtual std::optional<base::Time> GetLastSuccessTime() const = 0;

  // Returns the time until the next scheduled request. Returns std::nullopt if
  // there is no request scheduled.
  virtual std::optional<base::TimeDelta> GetTimeUntilNextRequest() const = 0;

  // Returns true after the |callback_| has been alerted of a request but before
  // HandleResult() is invoked.
  virtual bool IsWaitingForResult() const = 0;

  // The number of times the current request type has failed.
  // Once the request succeeds or a fresh request is made--for example,
  // via a manual request--this counter is reset.
  virtual size_t GetNumConsecutiveFailures() const = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  // Invokes |callback_|, alerting the owner that a request is ready.
  void NotifyOfRequest();

 private:
  bool is_running_ = false;
  OnRequestCallback callback_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_NEARBY_SCHEDULER_H_
