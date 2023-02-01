// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_TIMER_H_
#define COMPONENTS_COMPONENT_UPDATER_TIMER_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace component_updater {

class Timer {
 public:
  Timer();

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  ~Timer();

  void Start(base::TimeDelta initial_delay,
             base::TimeDelta delay,
             const base::RepeatingClosure& user_task);

  void Stop();

 private:
  void OnDelay();

  SEQUENCE_CHECKER(sequence_checker_);

  base::OneShotTimer timer_;

  base::TimeDelta delay_;
  base::RepeatingClosure user_task_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_TIMER_H_
