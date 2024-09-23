// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/next_idle_barrier.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/current_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/browser_thread.h"

namespace autofill {

struct NextIdleBarrier::Data {
  // Sets `value` to `true` and emits a metric how much time has passed since
  // `start_time`.
  void OnUiThreadIdle(base::TimeTicks start_time);

  // Starts a timer for a measurement attempt of the next idle time ticks.
  void StartTimerForMeasurementAttempt(base::TimeDelta delay);

  // Registers a callback with the UI thread in the browser process to measure
  // the time ticks the next time the UI thread becomes idle. Offset is passed
  // on to `SetValueToCurrentTimeTicksWithOffset`.
  void AttemptMeasurement();

  // Whether the UI thread has been idle.
  bool value = false;

  // The callback subscription for when the UI thread becomes idle the next
  // time.
  base::CallbackListSubscription on_idle_callback_subscription;

  // A timer that controls the next next measurement attempt.
  base::OneShotTimer measurement_delay_timer;
};

void NextIdleBarrier::Data::OnUiThreadIdle(base::TimeTicks start_time) {
  CHECK(!value);
  value = true;
  base::UmaHistogramTimes("Autofill.Popup.NextIdleTimeTicksDelay",
                          base::TimeTicks::Now() - start_time);
}

void NextIdleBarrier::Data::StartTimerForMeasurementAttempt(
    base::TimeDelta delay) {
  measurement_delay_timer.Start(FROM_HERE, delay, this,
                                &NextIdleBarrier::Data::AttemptMeasurement);
}

void NextIdleBarrier::Data::AttemptMeasurement() {
  on_idle_callback_subscription =
      base::CurrentUIThread::Get()->RegisterOnNextIdleCallback(
          {}, base::BindOnce(&NextIdleBarrier::Data::OnUiThreadIdle,
                             base::Unretained(this), base::TimeTicks::Now()));
}

NextIdleBarrier::NextIdleBarrier() = default;

NextIdleBarrier::NextIdleBarrier(NextIdleBarrier&&) = default;

NextIdleBarrier& NextIdleBarrier::operator=(NextIdleBarrier&&) = default;

NextIdleBarrier::~NextIdleBarrier() = default;

// static
NextIdleBarrier NextIdleBarrier::CreateNextIdleBarrierWithDelay(
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NextIdleBarrier result;
  result.data_ = std::make_unique<Data>();
  result.data_->StartTimerForMeasurementAttempt(delay);
  return result;
}

bool NextIdleBarrier::value() const {
  return data_ && data_->value;
}

}  // namespace autofill
