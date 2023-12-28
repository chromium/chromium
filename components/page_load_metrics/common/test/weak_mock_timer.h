// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_WEAK_MOCK_TIMER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_WEAK_MOCK_TIMER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/mock_timer.h"

namespace page_load_metrics {
namespace test {

// WeakMockTimer is a MockTimer that allows clients to keep WeakPtr<>s to it.
class WeakMockTimer final : public base::MockOneShotTimer {
 public:
  WeakMockTimer();
  ~WeakMockTimer() override;

  WeakMockTimer(const WeakMockTimer&) = delete;
  WeakMockTimer& operator=(const WeakMockTimer&) = delete;

  base::WeakPtr<WeakMockTimer> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WeakMockTimer> weak_ptr_factory_{this};
};

// WeakMockTimerProvider is a testing helper class that test classes can inherit
// from to provide basic MockTimer tracking capabilities.
class WeakMockTimerProvider {
 public:
  WeakMockTimerProvider();

  WeakMockTimerProvider(const WeakMockTimerProvider&) = delete;
  WeakMockTimerProvider& operator=(const WeakMockTimerProvider&) = delete;

  virtual ~WeakMockTimerProvider();

  base::MockOneShotTimer* GetMockTimer() const;
  void SetMockTimer(base::WeakPtr<WeakMockTimer> timer);

 private:
  base::WeakPtr<WeakMockTimer> timer_;
};

}  // namespace test
}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_WEAK_MOCK_TIMER_H_
