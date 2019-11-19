// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_WEAK_MOCK_TIMER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_WEAK_MOCK_TIMER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/mock_timer.h"

namespace page_load_metrics {
namespace test {

// WeakMockTimer is a MockTimer that allows clients to keep WeakPtr<>s to it.
class WeakMockTimer : public base::MockOneShotTimer,
                      public base::SupportsWeakPtr<WeakMockTimer> {
 public:
  WeakMockTimer();

 private:
  DISALLOW_COPY_AND_ASSIGN(WeakMockTimer);
};

// WeakMockTimerProvider is a testing helper class that test classes can inherit
// from to provide basic MockTimer tracking capabilities.
class WeakMockTimerProvider {
 public:
  WeakMockTimerProvider();
  virtual ~WeakMockTimerProvider();

  base::MockOneShotTimer* GetMockTimer() const;
  void SetMockTimer(base::WeakPtr<WeakMockTimer> timer);

 private:
  base::WeakPtr<WeakMockTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(WeakMockTimerProvider);
};

}  // namespace test
}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_WEAK_MOCK_TIMER_H_
