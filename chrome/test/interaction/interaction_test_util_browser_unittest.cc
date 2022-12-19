// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

class InteractionTestUtilBrowserTest : public views::ViewsTestBase {
 public:
  InteractionTestUtilBrowserTest() = default;
  ~InteractionTestUtilBrowserTest() override = default;

  std::unique_ptr<views::Widget> CreateWidget() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget->Init(std::move(params));
    auto* contents = widget->SetContentsView(std::make_unique<views::View>());
    auto* layout =
        contents->SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    layout->SetDefault(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
    views::test::WidgetVisibleWaiter visible_waiter(widget.get());
    widget->Show();
    visible_waiter.Wait();
    return widget;
  }

  static views::View* ElementToView(ui::TrackedElement* element) {
    return element ? element->AsA<views::TrackedElementViews>()->view()
                   : nullptr;
  }

  void SetUp() override {
    ViewsTestBase::SetUp();
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
    widget_ = CreateWidget();
    contents_ = widget_->GetContentsView();
  }

  void TearDown() override {
    widget_.reset();
    contents_ = nullptr;
    layout_provider_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::LayoutProvider> layout_provider_;
  InteractionTestUtilBrowser test_util_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> contents_ = nullptr;
};

TEST_F(InteractionTestUtilBrowserTest, PressHoverButton) {
  base::raw_ptr<HoverButton> hover_button;
  auto pressed = base::BindLambdaForTesting([&]() {
    hover_button->parent()->RemoveChildViewT(hover_button);
    hover_button = nullptr;
  });
  hover_button = contents_->AddChildView(std::make_unique<HoverButton>(
      views::Button::PressedCallback(pressed), u"Button"));
  widget_->LayoutRootViewIfNecessary();
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_.PressButton(
                views::ElementTrackerViews::GetInstance()->GetElementForView(
                    hover_button, true),
                ui::test::InteractionTestUtil::InputType::kKeyboard));
  EXPECT_EQ(nullptr, hover_button);
}
