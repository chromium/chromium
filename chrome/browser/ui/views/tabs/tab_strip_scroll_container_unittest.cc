// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include <cstddef>
#include <memory>
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "fake_base_tab_strip_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_border.h"

class TabStripScrollContainerTest : public ChromeViewsTestBase {
 public:
  TabStripScrollContainerTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    auto controller = std::make_unique<FakeBaseTabStripController>();
    controller_ = controller.get();
    auto tab_strip = std::make_unique<TabStrip>(std::move(controller));

    tab_strip_ = tab_strip.get();
    controller_->set_tab_strip(tab_strip_);
    root_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    root_widget_->Show();

    // root_widget_ takes ownership of the content_view
    tab_strip_scroll_container_ = root_widget_->SetContentsView(
        std::make_unique<TabStripScrollContainer>(std::move(tab_strip)));
  }

  void TearDown() override {
    root_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  ~TabStripScrollContainerTest() override = default;

 protected:
  raw_ptr<FakeBaseTabStripController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<TabStripScrollContainer, DanglingUntriaged>
      tab_strip_scroll_container_ = nullptr;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_ = nullptr;
  std::unique_ptr<views::Widget> root_widget_;
};

TEST_F(TabStripScrollContainerTest, AnchoredWidgetHidesOnScroll) {
  // set up the child widget
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_BUBBLE);
  params.bounds = gfx::Rect(0, 0, 400, 400);
  params.delegate = new views::BubbleDialogDelegate(
      tab_strip_, views::BubbleBorder::Arrow::LEFT_TOP);
  std::unique_ptr<views::Widget> widget_ = CreateTestWidget(std::move(params));
  widget_->Show();
  views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                    root_widget_->GetNativeView());

  EXPECT_TRUE(widget_->IsVisible());
  tab_strip_scroll_container_->OnContentsScrolledCallback();
  EXPECT_FALSE(widget_->IsVisible());
}
