// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_FAKE_NEARBY_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_FAKE_NEARBY_SCHEDULER_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler.h"

namespace ash::nearby {

// A fake implementation of NearbyScheduler that allows the user to set all
// scheduling data. It tracks the number of immediate requests and the handled
// results. The on-request callback can be invoked using
// InvokeRequestCallback().
class FakeNearbyScheduler : public NearbyScheduler {
 public:
  explicit FakeNearbyScheduler(OnRequestCallback callback);
  ~FakeNearbyScheduler() override;

  // NearbyScheduler:
  void MakeImmediateRequest() override;
  void HandleResult(bool success) override;
  void Reschedule() override;
  std::optional<base::Time> GetLastSuccessTime() const override;
  std::optional<base::TimeDelta> GetTimeUntilNextRequest() const override;
  bool IsWaitingForResult() const override;
  size_t GetNumConsecutiveFailures() const override;

  void SetLastSuccessTime(std::optional<base::Time> time);
  void SetTimeUntilNextRequest(std::optional<base::TimeDelta> time_delta);
  void SetIsWaitingForResult(bool is_waiting);
  void SetNumConsecutiveFailures(size_t num_failures);
  void SetHandleResultCallback(base::OnceClosure callback);

  void InvokeRequestCallback();

  size_t num_immediate_requests() const { return num_immediate_requests_; }
  size_t num_reschedule_calls() const { return num_reschedule_calls_; }
  const std::vector<bool>& handled_results() const { return handled_results_; }

 private:
  // NearbyScheduler:
  void OnStart() override;
  void OnStop() override;

  bool can_invoke_request_callback_ = false;
  size_t num_immediate_requests_ = 0;
  size_t num_reschedule_calls_ = 0;
  std::vector<bool> handled_results_;
  std::optional<base::Time> last_success_time_;
  std::optional<base::TimeDelta> time_until_next_request_;
  bool is_waiting_for_result_ = false;
  size_t num_consecutive_failures_ = 0;
  base::OnceClosure handle_result_callback_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_SCHEDULING_FAKE_NEARBY_SCHEDULER_H_
