// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_query_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

class ComposeboxQueryControllerTest : public testing::Test {
 public:
  ComposeboxQueryControllerTest() = default;
  ~ComposeboxQueryControllerTest() override = default;

  void SetUp() override {
    controller_ = std::make_unique<ComposeboxQueryController>();
  }

  ComposeboxQueryController& controller() { return *controller_; }

 private:
  std::unique_ptr<ComposeboxQueryController> controller_;
};

TEST_F(ComposeboxQueryControllerTest, NotifySessionStarted) {
  controller().NotifySessionStarted();
  EXPECT_EQ(SessionState::kSessionStarted, controller().session_state());
}

TEST_F(ComposeboxQueryControllerTest, NotifySessionAbandoned) {
  controller().NotifySessionAbandoned();
  EXPECT_EQ(SessionState::kSessionAbandoned, controller().session_state());
}
