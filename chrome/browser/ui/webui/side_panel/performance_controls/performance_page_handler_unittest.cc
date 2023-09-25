// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_page_handler.h"

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"

namespace {

class TestPerformancePageHandler : public PerformancePageHandler {
 public:
  TestPerformancePageHandler()
      : PerformancePageHandler(
            mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler>(),
            mojo::PendingRemote<side_panel::mojom::PerformancePage>(),
            static_cast<PerformanceSidePanelUI*>(nullptr)) {}
};

class PerformancePageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<TestPerformancePageHandler>();
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestPerformancePageHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<TestPerformancePageHandler> handler_;
};

TEST_F(PerformancePageHandlerTest, ConstructsHandler) {
  ASSERT_NE(handler(), nullptr);
}

}  // namespace
