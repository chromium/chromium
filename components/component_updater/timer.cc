// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/timer.h"

#include "base/bind.h"
#include "base/location.h"

namespace component_updater {

Timer::Timer() = default;

Timer::~Timer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Stop();
}

void Timer::Start(base::TimeDelta initial_delay,
                  base::TimeDelta delay,
                  const base::RepeatingClosure& user_task) {
  DCHECK(thread_checker_.CalledOnValidThread());

  delay_ = delay;
  user_task_ = user_task;

  timer_.Start(FROM_HERE, initial_delay,
               base::BindOnce(&Timer::OnDelay, base::Unretained(this)));
}

void Timer::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  timer_.Stop();
}

void Timer::OnDelay() {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_task_.Run();

  timer_.Start(FROM_HERE, delay_,
               base::BindOnce(&Timer::OnDelay, base::Unretained(this)));
}

}  // namespace component_updater
