// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/test_timer_factory.h"

#include "base/check.h"

namespace chromeos {
namespace tether {

TestTimerFactory::TestTimerFactory() = default;
TestTimerFactory::~TestTimerFactory() = default;

std::unique_ptr<base::OneShotTimer> TestTimerFactory::CreateOneShotTimer() {
  DCHECK(!device_id_for_next_timer_.empty());
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  device_id_to_timer_map_[device_id_for_next_timer_] = mock_timer.get();
  return std::move(mock_timer);
}

}  // namespace tether
}  // namespace chromeos