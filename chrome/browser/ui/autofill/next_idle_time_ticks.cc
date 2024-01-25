// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/next_idle_time_ticks.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/current_thread.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/browser_thread.h"

namespace autofill {

struct NextIdleTimeTicks::Data {
  void SetValueToCurrentTimeTicks();

  base::TimeTicks value;
  base::CallbackListSubscription on_idle_callback_subscription;

  // The `TimeTicks` when `this` was created - used for metrics reporting only.
  base::TimeTicks creation_time;
};

void NextIdleTimeTicks::Data::SetValueToCurrentTimeTicks() {
  value = base::TimeTicks::Now();
  if (base::FeatureList::IsEnabled(
          features::kAutofillPopupImprovedTimingChecks)) {
    CHECK(!creation_time.is_null());
    base::UmaHistogramTimes("Autofill.Popup.NextIdleTimeTicksDelay",
                            base::TimeTicks::Now() - creation_time);
  }
}

NextIdleTimeTicks::NextIdleTimeTicks() = default;

NextIdleTimeTicks::NextIdleTimeTicks(NextIdleTimeTicks&&) = default;

NextIdleTimeTicks& NextIdleTimeTicks::operator=(NextIdleTimeTicks&&) = default;

NextIdleTimeTicks::~NextIdleTimeTicks() = default;

// static
NextIdleTimeTicks NextIdleTimeTicks::CaptureNextIdleTimeTicks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NextIdleTimeTicks result;
  result.data_ = std::make_unique<Data>();
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillPopupImprovedTimingChecks)) {
    result.data_->creation_time = base::TimeTicks::Now();
    result.data_->on_idle_callback_subscription =
        base::CurrentUIThread::Get()->RegisterOnNextIdleCallback(
            {},
            base::BindOnce(&NextIdleTimeTicks::Data::SetValueToCurrentTimeTicks,
                           base::Unretained(result.data_.get())));
  } else {
    result.data_->SetValueToCurrentTimeTicks();
  }
  return result;
}

base::TimeTicks NextIdleTimeTicks::value() const {
  return data_ ? data_->value : base::TimeTicks();
}

}  // namespace autofill
