// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_ONE_SHOT_TIMER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_ONE_SHOT_TIMER_H_

#include "base/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/mock_timer.h"
#include "base/unguessable_token.h"

namespace ash::sync_wifi {

// Fake base::OneShotTimer implementation, which extends MockOneShotTimer and
// provides a mechanism for alerting its creator when it's destroyed.
class FakeOneShotTimer : public base::MockOneShotTimer {
 public:
  FakeOneShotTimer(base::OnceCallback<void(const base::UnguessableToken&)>
                       destructor_callback);

  FakeOneShotTimer(const FakeOneShotTimer&) = delete;
  FakeOneShotTimer& operator=(const FakeOneShotTimer&) = delete;

  ~FakeOneShotTimer() override;

  const base::UnguessableToken& id() const { return id_; }

 private:
  base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback_;
  base::UnguessableToken id_;
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_ONE_SHOT_TIMER_H_
