// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/test_support/technology_enablement_waiter.h"

#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TechnologyEnablementWaiter::TechnologyEnablementWaiter(
    NetworkStateHandler* network_state_handler)
    : network_state_handler_(network_state_handler) {
  network_state_handler_observer_.Observe(network_state_handler_.get());
}

TechnologyEnablementWaiter::~TechnologyEnablementWaiter() = default;

void TechnologyEnablementWaiter::Wait(NetworkTypePattern type_pattern,
                                      bool waiting_state) {
  ASSERT_FALSE(run_loop_.has_value())
      << "Nested TechnologyEnablementWaiter::Wait not supported";

  type_pattern_ = type_pattern;
  waiting_state_ = waiting_state;

  if (IsConditionFulfilled()) {
    return;
  }

  run_loop_.emplace();
  run_loop_->Run();
  run_loop_.reset();
}

void TechnologyEnablementWaiter::DeviceListChanged() {
  if (run_loop_.has_value() && IsConditionFulfilled()) {
    run_loop_->Quit();
  }
}

bool TechnologyEnablementWaiter::IsConditionFulfilled() {
  bool enabled = network_state_handler_->IsTechnologyEnabled(type_pattern_);
  return enabled == waiting_state_;
}

}  // namespace ash
