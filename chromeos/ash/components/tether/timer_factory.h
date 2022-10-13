// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TIMER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TIMER_FACTORY_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"

namespace ash {

namespace tether {

// Serves as a simple Timer creator, injected into classes that use Timers.
// Is intended to be overriden during testing in order to stub or mock the
// Timers used by the object under test.
class TimerFactory {
 public:
  virtual ~TimerFactory();

  virtual std::unique_ptr<base::OneShotTimer> CreateOneShotTimer();
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TIMER_FACTORY_H_
