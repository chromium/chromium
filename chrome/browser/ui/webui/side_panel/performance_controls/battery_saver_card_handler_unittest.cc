// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/webui/side_panel/performance_controls/battery_saver_card_handler.h"

#include "chrome/test/base/browser_with_test_window_test.h"

namespace {

class TestBatterySaverCardHandler : public BatterySaverCardHandler {
 public:
  TestBatterySaverCardHandler()
      : BatterySaverCardHandler(
            mojo::PendingReceiver<side_panel::mojom::BatterySaverCardHandler>(),
            mojo::PendingRemote<side_panel::mojom::BatterySaverCard>()) {}
};

class BatterySaverCardHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<TestBatterySaverCardHandler>();
  }

  void TearDown() override {
    handler_.reset();
    browser()->tab_strip_model()->CloseAllTabs();
    BrowserWithTestWindowTest::TearDown();
  }

  TestBatterySaverCardHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<TestBatterySaverCardHandler> handler_;
};

TEST_F(BatterySaverCardHandlerTest, ConstructsHandler) {
  ASSERT_NE(handler(), nullptr);
}

}  // namespace
