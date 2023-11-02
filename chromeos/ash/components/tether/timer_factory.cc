// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/timer_factory.h"

#include <memory>

namespace ash {

namespace tether {

TimerFactory::~TimerFactory() = default;

std::unique_ptr<base::OneShotTimer> TimerFactory::CreateOneShotTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace tether

}  // namespace ash
