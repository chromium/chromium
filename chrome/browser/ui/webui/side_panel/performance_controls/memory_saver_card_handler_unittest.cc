// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/webui/side_panel/performance_controls/memory_saver_card_handler.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/test_web_ui.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"

namespace {

class MockPerformanceSidePanelUI : public PerformanceSidePanelUI {
 public:
  explicit MockPerformanceSidePanelUI(content::WebUI* test_web_ui)
      : PerformanceSidePanelUI(test_web_ui) {}
};

class TestMemorySaverCardHandler : public MemorySaverCardHandler {
 public:
  explicit TestMemorySaverCardHandler(
      PerformanceSidePanelUI* performance_side_panel_ui)
      : MemorySaverCardHandler(
            mojo::PendingReceiver<side_panel::mojom::MemorySaverCardHandler>(),
            mojo::PendingRemote<side_panel::mojom::MemorySaverCard>(),
            performance_side_panel_ui) {}
};

class MemorySaverCardHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_.get());
    performance_side_panel_ui_ =
        std::make_unique<MockPerformanceSidePanelUI>(test_web_ui_.get());
    handler_ = std::make_unique<TestMemorySaverCardHandler>(
        performance_side_panel_ui_.get());
  }

  void TearDown() override {
    handler_.reset();
    performance_side_panel_ui_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    browser()->tab_strip_model()->CloseAllTabs();
    BrowserWithTestWindowTest::TearDown();
  }

  TestMemorySaverCardHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<TestMemorySaverCardHandler> handler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<MockPerformanceSidePanelUI> performance_side_panel_ui_;
};

TEST_F(MemorySaverCardHandlerTest, ConstructsHandler) {
  ASSERT_NE(handler(), nullptr);
}

}  // namespace
