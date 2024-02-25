// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/test/weak_mock_timer.h"

namespace page_load_metrics {
namespace test {

WeakMockTimer::WeakMockTimer() = default;
WeakMockTimer::~WeakMockTimer() = default;

WeakMockTimerProvider::WeakMockTimerProvider() = default;
WeakMockTimerProvider::~WeakMockTimerProvider() = default;

base::MockOneShotTimer* WeakMockTimerProvider::GetMockTimer() const {
  return timer_.get();
}

void WeakMockTimerProvider::SetMockTimer(base::WeakPtr<WeakMockTimer> timer) {
  timer_ = timer;
}

}  // namespace test
}  // namespace page_load_metrics
