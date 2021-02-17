// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keep_alive_registry/keep_alive_registry.h"

#include <memory>

#include "components/keep_alive_registry/keep_alive_state_observer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "testing/gtest/include/gtest/gtest.h"

class KeepAliveRegistryTest : public testing::Test,
                              public KeepAliveStateObserver {
 public:
  KeepAliveRegistryTest()
      : on_restart_allowed_call_count_(0),
        on_restart_forbidden_call_count_(0),
        start_keep_alive_call_count_(0),
        stop_keep_alive_call_count_(0),
        registry_(KeepAliveRegistry::GetInstance()) {
    registry_->AddObserver(this);

    EXPECT_FALSE(registry_->IsKeepingAlive());
  }

  ~KeepAliveRegistryTest() override {
    registry_->RemoveObserver(this);

    EXPECT_FALSE(registry_->IsKeepingAlive());
  }

  void OnKeepAliveStateChanged(bool is_keeping_alive) override {
    if (is_keeping_alive)
      ++start_keep_alive_call_count_;
    else
      ++stop_keep_alive_call_count_;
  }

  void OnKeepAliveRestartStateChanged(bool can_restart) override {
    if (can_restart)
      ++on_restart_allowed_call_count_;
    else
      ++on_restart_forbidden_call_count_;
  }

 protected:
  int on_restart_allowed_call_count_;
  int on_restart_forbidden_call_count_;
  int start_keep_alive_call_count_;
  int stop_keep_alive_call_count_;
  KeepAliveRegistry* registry_;
};

// Test the IsKeepingAlive state and when we interact with the browser with
// a KeepAlive registered.
TEST_F(KeepAliveRegistryTest, BasicKeepAliveTest) {
  EXPECT_EQ(0, start_keep_alive_call_count_);
  EXPECT_EQ(0, stop_keep_alive_call_count_);

  {
    // Arbitrarily chosen Origin
    ScopedKeepAlive test_keep_alive(KeepAliveOrigin::CHROME_APP_DELEGATE,
                                    KeepAliveRestartOption::DISABLED);

    // We should require the browser to stay alive
    ASSERT_EQ(1, start_keep_alive_call_count_--);  // decrement to ack
    EXPECT_TRUE(registry_->IsKeepingAlive());
  }

  // We should be back to normal now, notifying of the state change.
  ASSERT_EQ(1, stop_keep_alive_call_count_--);
  EXPECT_FALSE(registry_->IsKeepingAlive());

  // This should not have changed.
  ASSERT_EQ(0, start_keep_alive_call_count_);
}

// Test the IsKeepingAlive state and when we interact with the browser with
// more than one KeepAlive registered.
TEST_F(KeepAliveRegistryTest, DoubleKeepAliveTest) {
  EXPECT_EQ(0, start_keep_alive_call_count_);
  EXPECT_EQ(0, stop_keep_alive_call_count_);
  std::unique_ptr<ScopedKeepAlive> keep_alive_1, keep_alive_2;

  keep_alive_1.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE,
                                         KeepAliveRestartOption::DISABLED));
  ASSERT_EQ(1, start_keep_alive_call_count_--);  // decrement to ack
  EXPECT_TRUE(registry_->IsKeepingAlive());

  keep_alive_2.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE,
                                         KeepAliveRestartOption::DISABLED));
  // We should not increment the count twice
  EXPECT_EQ(0, start_keep_alive_call_count_);
  EXPECT_TRUE(registry_->IsKeepingAlive());

  keep_alive_1.reset();
  // We should not decrement the count before the last keep alive is released.
  EXPECT_EQ(0, stop_keep_alive_call_count_);
  EXPECT_TRUE(registry_->IsKeepingAlive());

  keep_alive_2.reset();
  ASSERT_EQ(1, stop_keep_alive_call_count_--);
  EXPECT_EQ(0, start_keep_alive_call_count_);
  EXPECT_FALSE(registry_->IsKeepingAlive());
}

// Test the IsKeepingAlive state and when we interact with the browser with
// more than one KeepAlive registered.
TEST_F(KeepAliveRegistryTest, RestartOptionTest) {
  std::unique_ptr<ScopedKeepAlive> keep_alive, keep_alive_restart;

  EXPECT_EQ(0, on_restart_allowed_call_count_);
  EXPECT_EQ(0, on_restart_forbidden_call_count_);

  // With a normal keep alive, restart should not be allowed
  keep_alive.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE,
                                       KeepAliveRestartOption::DISABLED));
  ASSERT_EQ(1, on_restart_forbidden_call_count_--);  // decrement to ack

  // Restart should not be allowed if all KA don't allow it.
  keep_alive_restart.reset(new ScopedKeepAlive(
      KeepAliveOrigin::CHROME_APP_DELEGATE, KeepAliveRestartOption::ENABLED));
  EXPECT_EQ(0, on_restart_allowed_call_count_);

  // Now restart should be allowed, the only one left allows it.
  keep_alive.reset();
  ASSERT_EQ(1, on_restart_allowed_call_count_--);

  // No keep alive, we should no prevent restarts.
  keep_alive.reset();
  EXPECT_EQ(0, on_restart_forbidden_call_count_);

  // Make sure all calls were checked.
  EXPECT_EQ(0, on_restart_allowed_call_count_);
  EXPECT_EQ(0, on_restart_forbidden_call_count_);
}

TEST_F(KeepAliveRegistryTest, WouldRestartWithoutTest) {
  // WouldRestartWithout() should have the same results as IsRestartAllowed()
  // when called with an empty vector.
  std::vector<KeepAliveOrigin> empty_vector;

  // Init and sanity checks.
  ScopedKeepAlive kar(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                      KeepAliveRestartOption::ENABLED);
  ASSERT_TRUE(registry_->IsRestartAllowed());
  EXPECT_EQ(registry_->IsRestartAllowed(),
            registry_->WouldRestartWithout(empty_vector));
  ScopedKeepAlive ka1(KeepAliveOrigin::CHROME_APP_DELEGATE,
                      KeepAliveRestartOption::DISABLED);
  ASSERT_FALSE(registry_->IsRestartAllowed());

  // Basic case: exclude one KeepAlive.
  EXPECT_TRUE(
      registry_->WouldRestartWithout({KeepAliveOrigin::CHROME_APP_DELEGATE}));
  EXPECT_FALSE(registry_->WouldRestartWithout(
      {KeepAliveOrigin::BACKGROUND_MODE_MANAGER}));

  // Check it works properly with multiple KeepAlives of the same type
  ScopedKeepAlive ka2(KeepAliveOrigin::CHROME_APP_DELEGATE,
                      KeepAliveRestartOption::DISABLED);
  EXPECT_TRUE(
      registry_->WouldRestartWithout({KeepAliveOrigin::CHROME_APP_DELEGATE}));

  // Check it works properly with different KeepAlive types
  ScopedKeepAlive ka3(KeepAliveOrigin::PANEL, KeepAliveRestartOption::DISABLED);
  EXPECT_FALSE(
      registry_->WouldRestartWithout({KeepAliveOrigin::CHROME_APP_DELEGATE}));
  EXPECT_FALSE(registry_->WouldRestartWithout(
      {KeepAliveOrigin::BACKGROUND_MODE_MANAGER}));
  EXPECT_TRUE(registry_->WouldRestartWithout(
      {KeepAliveOrigin::CHROME_APP_DELEGATE, KeepAliveOrigin::PANEL}));
  EXPECT_EQ(registry_->IsRestartAllowed(),
            registry_->WouldRestartWithout(empty_vector));
}
