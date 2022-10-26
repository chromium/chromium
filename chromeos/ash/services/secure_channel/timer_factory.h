// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_

#include <memory>

namespace base {
class OneShotTimer;
}

namespace ash::secure_channel {

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

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_
