// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_DELAY_BASED_TIME_SOURCE_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_DELAY_BASED_TIME_SOURCE_H_

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/viz_common_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}
class SingleThreadTaskRunner;
}  // namespace base

namespace viz {
class VIZ_COMMON_EXPORT DelayBasedTimeSourceClient {
 public:
  virtual void OnTimerTick() = 0;

 protected:
  virtual ~DelayBasedTimeSourceClient() = default;
};

// This timer implements a time source that achieves the specified interval
// in face of millisecond-precision delayed callbacks and random queueing
// delays. DelayBasedTimeSource uses base::TimeTicks::Now as its timebase.
class VIZ_COMMON_EXPORT DelayBasedTimeSource {
 public:
  explicit DelayBasedTimeSource(base::SingleThreadTaskRunner* task_runner);

  DelayBasedTimeSource(const DelayBasedTimeSource&) = delete;
  DelayBasedTimeSource& operator=(const DelayBasedTimeSource&) = delete;

  virtual ~DelayBasedTimeSource();

  void SetClient(DelayBasedTimeSourceClient* client);

  void SetTimebaseAndInterval(base::TimeTicks timebase,
                              base::TimeDelta interval);

  base::TimeDelta Interval() const;

  void SetActive(bool active);
  bool Active() const;

  // Get the last and next tick times. NextTickTime() returns null when
  // inactive.
  base::TimeTicks LastTickTime() const;
  base::TimeTicks NextTickTime() const;

  virtual void AsValueInto(base::trace_event::TracedValue* dict) const;

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now() const;
  virtual std::string TypeString() const;

 private:
  void PostNextTickTask(base::TimeTicks now);
  void ResetTickTask(base::TimeTicks now);

  void OnTimerTick();

  raw_ptr<DelayBasedTimeSourceClient> client_;

  bool active_;

  base::TimeTicks timebase_;
  base::TimeDelta interval_;

  base::TimeTicks last_tick_time_;
  base::TimeTicks next_tick_time_;

  raw_ptr<base::SingleThreadTaskRunner> task_runner_;

  base::RepeatingClosure tick_closure_;
  base::DeadlineTimer timer_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_DELAY_BASED_TIME_SOURCE_H_
