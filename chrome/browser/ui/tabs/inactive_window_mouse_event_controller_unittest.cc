// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

class InactiveWindowMouseEventControllerTest : public ::testing::Test {};

TEST_F(InactiveWindowMouseEventControllerTest, DefaultShouldNotAccept) {
  InactiveWindowMouseEventController capabilities;
  EXPECT_FALSE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
}

TEST_F(InactiveWindowMouseEventControllerTest,
       AcceptMouseEventsIncrementsCounter) {
  InactiveWindowMouseEventController capabilities;
  {
    auto scope = capabilities.AcceptMouseEventsWhileWindowInactive();
    EXPECT_TRUE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
  }
  EXPECT_FALSE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
}

TEST_F(InactiveWindowMouseEventControllerTest, MultipleScopesIncrementCounter) {
  InactiveWindowMouseEventController capabilities;
  {
    auto scope1 = capabilities.AcceptMouseEventsWhileWindowInactive();
    EXPECT_TRUE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
    {
      auto scope2 = capabilities.AcceptMouseEventsWhileWindowInactive();
      EXPECT_TRUE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
    }
    EXPECT_TRUE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
  }
  EXPECT_FALSE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
}

}  // namespace tabs
