// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_page_handler.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/test_web_ui.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"

namespace {

class MockEmbedder : public ui::MojoBubbleWebUIController::Embedder {
 public:
  virtual ~MockEmbedder() = default;

  void ShowUI() override { show_ui_called_ = true; }
  void CloseUI() override {}
  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override {}
  void HideContextMenu() override {}

  bool GetShowUICalledForTesting() { return show_ui_called_; }

  base::WeakPtr<MockEmbedder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool show_ui_called_ = false;

  base::WeakPtrFactory<MockEmbedder> weak_ptr_factory_{this};
};

class MockPerformanceSidePanelUI : public PerformanceSidePanelUI {
 public:
  explicit MockPerformanceSidePanelUI(content::WebUI* test_web_ui)
      : PerformanceSidePanelUI(test_web_ui) {}
};

class TestPerformancePageHandler : public PerformancePageHandler {
 public:
  explicit TestPerformancePageHandler(
      PerformanceSidePanelUI* performance_side_panel_ui)
      : PerformancePageHandler(
            mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler>(),
            mojo::PendingRemote<side_panel::mojom::PerformancePage>(),
            performance_side_panel_ui) {}
};

class PerformancePageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_.get());
    performance_side_panel_ui_ =
        std::make_unique<MockPerformanceSidePanelUI>(test_web_ui_.get());
    handler_ = std::make_unique<TestPerformancePageHandler>(
        performance_side_panel_ui_.get());

    embedder_ = std::make_unique<MockEmbedder>();
    performance_side_panel_ui_->set_embedder(embedder_->GetWeakPtr());
  }

  void TearDown() override {
    handler_.reset();
    embedder_.reset();
    performance_side_panel_ui_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    browser()->tab_strip_model()->CloseAllTabs();
    BrowserWithTestWindowTest::TearDown();
  }

  TestPerformancePageHandler* handler() { return handler_.get(); }
  MockEmbedder* embedder() { return embedder_.get(); }

 private:
  std::unique_ptr<TestPerformancePageHandler> handler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<MockPerformanceSidePanelUI> performance_side_panel_ui_;
  std::unique_ptr<MockEmbedder> embedder_;
};

TEST_F(PerformancePageHandlerTest, ConstructsHandler) {
  ASSERT_NE(handler(), nullptr);
}

TEST_F(PerformancePageHandlerTest, CallsShowUI) {
  PerformancePageHandler* performance_page_handler = handler();
  performance_page_handler->ShowUI();
  ASSERT_TRUE(embedder()->GetShowUICalledForTesting());
}

}  // namespace
