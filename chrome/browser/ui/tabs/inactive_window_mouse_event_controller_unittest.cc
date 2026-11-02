// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"

#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

class InactiveWindowMouseEventControllerTest : public ::testing::Test {};

TEST_F(InactiveWindowMouseEventControllerTest, DefaultShouldNotAccept) {
  MockTabInterface tab;
  EXPECT_EQ(InactiveWindowMouseEventController::From(nullptr), nullptr);
  EXPECT_EQ(InactiveWindowMouseEventController::From(&tab), nullptr);
  {
    InactiveWindowMouseEventController capabilities(tab);
    EXPECT_EQ(InactiveWindowMouseEventController::From(&tab), &capabilities);
    EXPECT_FALSE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
  }
  EXPECT_EQ(InactiveWindowMouseEventController::From(&tab), nullptr);
}

TEST_F(InactiveWindowMouseEventControllerTest,
       AcceptMouseEventsIncrementsCounter) {
  MockTabInterface tab;
  InactiveWindowMouseEventController capabilities(tab);
  {
    auto scope = capabilities.AcceptMouseEventsWhileWindowInactive();
    EXPECT_TRUE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
  }
  EXPECT_FALSE(capabilities.ShouldAcceptMouseEventsWhileWindowInactive());
}

TEST_F(InactiveWindowMouseEventControllerTest, MultipleScopesIncrementCounter) {
  MockTabInterface tab;
  InactiveWindowMouseEventController capabilities(tab);
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
