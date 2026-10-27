// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_palette_bubble_controller.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class MemoryBanksPaletteBubbleControllerTest : public ChromeViewsTestBase {
 public:
  MemoryBanksPaletteBubbleControllerTest() = default;
  ~MemoryBanksPaletteBubbleControllerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void ShowBubbleWithAnchor(MemoryBanksPaletteBubbleController* controller,
                            views::BubbleAnchor anchor) {
    controller->ShowBubbleWithAnchor(anchor);
  }

  views::Widget* GetBubbleWidget(
      MemoryBanksPaletteBubbleController* controller) {
    return controller->bubble_widget_.get();
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<views::Widget> anchor_widget_;
};

namespace {

TEST_F(MemoryBanksPaletteBubbleControllerTest, ShowAndCloseBubble) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  auto* controller =
      MemoryBanksPaletteBubbleController::GetOrCreateForWebContents(
          web_contents.get());
  ASSERT_NE(nullptr, controller);

  // Without a TabInterface, ShowBubble() safely no-ops.
  controller->ShowBubble();
  EXPECT_EQ(nullptr, GetBubbleWidget(controller));

  ShowBubbleWithAnchor(controller,
                       views::BubbleAnchor(anchor_widget_->GetContentsView()));
  views::Widget* first_widget = GetBubbleWidget(controller);
  ASSERT_NE(nullptr, first_widget);
  EXPECT_FALSE(first_widget->IsClosed());

  // Calling ShowBubbleWithAnchor() while already open activates the existing
  // widget.
  ShowBubbleWithAnchor(controller,
                       views::BubbleAnchor(anchor_widget_->GetContentsView()));
  EXPECT_EQ(first_widget, GetBubbleWidget(controller));

  controller->CloseBubble();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetBubbleWidget(controller) == nullptr; }));
}

TEST_F(MemoryBanksPaletteBubbleControllerTest, NavigationClosesBubble) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  auto* controller =
      MemoryBanksPaletteBubbleController::GetOrCreateForWebContents(
          web_contents.get());
  ShowBubbleWithAnchor(controller,
                       views::BubbleAnchor(anchor_widget_->GetContentsView()));
  ASSERT_NE(nullptr, GetBubbleWidget(controller));

  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetBubbleWidget(controller) == nullptr; }));
}

TEST_F(MemoryBanksPaletteBubbleControllerTest,
       DestroyingWebContentsWhileClosingDoesNotCrash) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  auto* controller =
      MemoryBanksPaletteBubbleController::GetOrCreateForWebContents(
          web_contents.get());
  ShowBubbleWithAnchor(controller,
                       views::BubbleAnchor(anchor_widget_->GetContentsView()));
  ASSERT_NE(nullptr, GetBubbleWidget(controller));

  // Close the bubble (which posts a deferred ResetBubble() task) and
  // immediately destroy the WebContents and Profile in the same task turn.
  controller->CloseBubble();
  web_contents.reset();
  profile_.reset();

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace
