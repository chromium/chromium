// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_NEXT_IDLE_TIME_TICKS_H_
#define CHROME_BROWSER_UI_AUTOFILL_NEXT_IDLE_TIME_TICKS_H_

#include <memory>

#include "base/time/time.h"

namespace autofill {

class NextIdleTimeTicks {
 public:
  NextIdleTimeTicks();
  NextIdleTimeTicks(const NextIdleTimeTicks&) = delete;
  NextIdleTimeTicks(NextIdleTimeTicks&&);
  NextIdleTimeTicks& operator=(const NextIdleTimeTicks&) = delete;
  NextIdleTimeTicks& operator=(NextIdleTimeTicks&&);
  ~NextIdleTimeTicks();

  // Returns a `NextIdleTimeTicks` whose value is set to `base::TimeTicks::Now`
  // the next time the current UI thread is idle - until then, its value is
  // null. Note that this is currently guarded behind the
  // `autofill::features::kAutofillPopupImprovedTimingChecks` feature. While the
  // feature is disabled, it defaults to measuring the time immediately.
  static NextIdleTimeTicks CaptureNextIdleTimeTicks();

  // The first `TimeTicks` at which the UI thread this `NextIdleTimeTicks` was
  // created on then became idle. `is_null()` if this has not occurred yet.
  base::TimeTicks value() const;

 private:
  struct Data;

  // Container for the value and a callback list subscription. It is wrapped in
  // its own struct and allocated on the heap to ensure that it remains alive
  // and keeps the same memory address even if the parent `NextIdleTimeTicks` is
  // move-assigned.
  std::unique_ptr<Data> data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_NEXT_IDLE_TIME_TICKS_H_
