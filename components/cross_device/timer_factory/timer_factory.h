// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CROSS_DEVICE_TIMER_FACTORY_TIMER_FACTORY_H_
#define COMPONENTS_CROSS_DEVICE_TIMER_FACTORY_TIMER_FACTORY_H_

#include <memory>

namespace base {
class OneShotTimer;
}

namespace cross_device {

// Creates timers. This class is needed so that tests can inject test doubles
// for timers.
class TimerFactory {
 public:
  TimerFactory(const TimerFactory&) = delete;
  TimerFactory& operator=(const TimerFactory&) = delete;

  virtual ~TimerFactory() = default;
  virtual std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() = 0;

 protected:
  TimerFactory() = default;
};

}  // namespace cross_device

#endif  // COMPONENTS_CROSS_DEVICE_TIMER_FACTORY_TIMER_FACTORY_H_
