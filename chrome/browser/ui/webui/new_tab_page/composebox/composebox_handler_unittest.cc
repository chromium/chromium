// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockQueryController : public ComposeboxQueryController {
 public:
  MockQueryController() = default;
  ~MockQueryController() override = default;

  MOCK_METHOD(void, NotifySessionStarted, ());
  MOCK_METHOD(void, NotifySessionAbandoned, ());
};

class ComposeboxHandlerTest : public testing::Test {
 public:
  ComposeboxHandlerTest() = default;
  ~ComposeboxHandlerTest() override = default;

  void SetUp() override {
    auto query_controller_ptr = std::make_unique<MockQueryController>();
    query_controller_ = query_controller_ptr.get();
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler>(),
        std::move(query_controller_ptr));
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }

 private:
  std::unique_ptr<ComposeboxHandler> handler_;
  raw_ptr<MockQueryController> query_controller_;
};

TEST_F(ComposeboxHandlerTest, NotifySessionStarted) {
  EXPECT_CALL(query_controller(), NotifySessionStarted).Times(1);
  handler().NotifySessionStarted();
}
