// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_NEXT_IDLE_BARRIER_H_
#define CHROME_BROWSER_UI_AUTOFILL_NEXT_IDLE_BARRIER_H_

#include <memory>

#include "base/time/time.h"

namespace autofill {

class NextIdleBarrier {
 public:
  NextIdleBarrier();
  NextIdleBarrier(const NextIdleBarrier&) = delete;
  NextIdleBarrier(NextIdleBarrier&&);
  NextIdleBarrier& operator=(const NextIdleBarrier&) = delete;
  NextIdleBarrier& operator=(NextIdleBarrier&&);
  ~NextIdleBarrier();

  // Returns a `NextIdleBarrier` that is evaluated after `delay`. That is, its
  // value becomes `true` once the `delay` has passed and the UI thread has
  // become idle afterwards.
  static NextIdleBarrier CreateNextIdleBarrierWithDelay(base::TimeDelta delay);

  // Indicates whether the barrier has been passed, i.e. whether the UI thread
  // has been idle at least once.
  bool value() const;

 private:
  struct Data;

  // Container for the value and a callback list subscription. It is wrapped in
  // its own struct and allocated on the heap to ensure that it remains alive
  // and keeps the same memory address even if the parent `NextIdleTimeTicks` is
  // move-assigned.
  std::unique_ptr<Data> data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_NEXT_IDLE_BARRIER_H_
