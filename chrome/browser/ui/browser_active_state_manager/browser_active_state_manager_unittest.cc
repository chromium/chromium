// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserActiveStateManagerTest : public testing::Test {
 public:
  BrowserActiveStateManagerTest() = default;
  ~BrowserActiveStateManagerTest() override = default;

 protected:
  MockBrowserWindowInterface mock_browser_;
};

TEST_F(BrowserActiveStateManagerTest, FromReturnsInstance) {
  BrowserActiveStateManager manager(mock_browser_);
  EXPECT_EQ(&manager, BrowserActiveStateManager::From(&mock_browser_));
}

TEST_F(BrowserActiveStateManagerTest, InitialStateInactive) {
  BrowserActiveStateManager manager(mock_browser_);
  EXPECT_FALSE(manager.IsActive());
}

TEST_F(BrowserActiveStateManagerTest, StateChangeAndCallbacks) {
  BrowserActiveStateManager manager(mock_browser_);

  base::MockCallback<BrowserWindowInterface::DidBecomeActiveCallback>
      active_callback;
  base::MockCallback<BrowserWindowInterface::DidBecomeInactiveCallback>
      inactive_callback;

  auto active_sub = manager.RegisterDidBecomeActive(active_callback.Get());
  auto inactive_sub =
      manager.RegisterDidBecomeInactive(inactive_callback.Get());

  EXPECT_CALL(active_callback, Run(&mock_browser_)).Times(1);
  manager.DidBecomeActive();
  EXPECT_TRUE(manager.IsActive());

  // Calling DidBecomeActive while already active should do nothing extra.
  manager.DidBecomeActive();
  EXPECT_TRUE(manager.IsActive());

  EXPECT_CALL(inactive_callback, Run(&mock_browser_)).Times(1);
  manager.DidBecomeInactive();
  EXPECT_FALSE(manager.IsActive());

  // Calling DidBecomeInactive while already inactive should do nothing extra.
  manager.DidBecomeInactive();
  EXPECT_FALSE(manager.IsActive());
}
