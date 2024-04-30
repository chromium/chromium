// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/next_idle_time_ticks.h"

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

struct NextIdleTimeTicks::Data {
  // Sets `value` to the current time ticks.
  void SetValueToCurrentTimeTicks();

  // Starts a timer for a measurement attempt of the next idle time ticks.
  void StartTimerForMeasurementAttempt(base::TimeDelta delay);

  // Registers a callback with the UI thread in the browser process to measure
  // the time ticks the next time the UI thread becomes idle. Offset is passed
  // on to `SetValueToCurrentTimeTicksWithOffset`.
  void AttemptMeasurement();

  base::TimeTicks value;

  // The delay after which the first measurement attempt is made.
  base::TimeDelta measurement_start_delay;

  // The callback subscription for when the UI thread becomes idle the next
  // time.
  base::CallbackListSubscription on_idle_callback_subscription;

  // The `TimeTicks` when the measurement attempt was started - used for metrics
  // reporting only.
  base::TimeTicks start_time_measurement_attempt;

  // A timer that controls the next next measurement attempt.
  base::OneShotTimer measurement_delay_timer;
};

void NextIdleTimeTicks::Data::SetValueToCurrentTimeTicks() {
  value = base::TimeTicks::Now();
  if (base::FeatureList::IsEnabled(
          features::kAutofillPopupImprovedTimingChecksV2)) {
    CHECK(!start_time_measurement_attempt.is_null());
    base::UmaHistogramTimes(
        "Autofill.Popup.NextIdleTimeTicksDelay",
        base::TimeTicks::Now() - start_time_measurement_attempt);
  }
}

void NextIdleTimeTicks::Data::StartTimerForMeasurementAttempt(
    base::TimeDelta delay) {
  measurement_start_delay = delay;
  measurement_delay_timer.Start(FROM_HERE, delay, this,
                                &NextIdleTimeTicks::Data::AttemptMeasurement);
}

void NextIdleTimeTicks::Data::AttemptMeasurement() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillPopupImprovedTimingChecksV2)) {
    start_time_measurement_attempt = base::TimeTicks::Now();
    on_idle_callback_subscription =
        base::CurrentUIThread::Get()->RegisterOnNextIdleCallback(
            {},
            base::BindOnce(&NextIdleTimeTicks::Data::SetValueToCurrentTimeTicks,
                           base::Unretained(this)));
  } else {
    SetValueToCurrentTimeTicks();
  }
}

NextIdleTimeTicks::NextIdleTimeTicks() = default;

NextIdleTimeTicks::NextIdleTimeTicks(NextIdleTimeTicks&&) = default;

NextIdleTimeTicks& NextIdleTimeTicks::operator=(NextIdleTimeTicks&&) = default;

NextIdleTimeTicks::~NextIdleTimeTicks() = default;

// static
NextIdleTimeTicks NextIdleTimeTicks::CaptureNextIdleTimeTicksWithDelay(
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NextIdleTimeTicks result;
  result.data_ = std::make_unique<Data>();
  result.data_->StartTimerForMeasurementAttempt(delay);
  return result;
}

base::TimeTicks NextIdleTimeTicks::value() const {
  return data_ && !data_->value.is_null()
             ? data_->measurement_start_delay + data_->value
             : base::TimeTicks();
}

}  // namespace autofill
