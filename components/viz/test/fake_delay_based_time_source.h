// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_DELAY_BASED_TIME_SOURCE_H_
#define COMPONENTS_VIZ_TEST_FAKE_DELAY_BASED_TIME_SOURCE_H_

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace base {
class TickClock;
}

namespace viz {

class FakeDelayBasedTimeSourceClient : public DelayBasedTimeSourceClient {
 public:
  FakeDelayBasedTimeSourceClient() : tick_called_(false) {}
  void Reset() { tick_called_ = false; }
  bool TickCalled() const { return tick_called_; }

  // DelayBasedTimeSourceClient implementation.
  void OnTimerTick() override;

 protected:
  bool tick_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDelayBasedTimeSourceClient);
};

class FakeDelayBasedTimeSource : public DelayBasedTimeSource {
 public:
  FakeDelayBasedTimeSource(const base::TickClock* now_src,
                           base::SingleThreadTaskRunner* task_runner);
  ~FakeDelayBasedTimeSource() override;

  // Overridden from DelayBasedTimeSource
  base::TimeTicks Now() const override;
  std::string TypeString() const override;

 private:
  // Not owned.
  const base::TickClock* now_src_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelayBasedTimeSource);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_DELAY_BASED_TIME_SOURCE_H_
