// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include <gtest/gtest.h>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace {
// Toolbar button size is ~34dp.
constexpr gfx::Size kButtonSize(34, 34);

class MockToolbarController : public ToolbarController {
 public:
  MockToolbarController(std::vector<ui::ElementIdentifier> element_ids,
                        PopOutIdentifierMap pop_out_identifier_map,
                        int element_flex_order_start,
                        views::View* toolbar_container_view,
                        views::View* overflow_button)
      : ToolbarController(element_ids,
                          pop_out_identifier_map,
                          element_flex_order_start,
                          toolbar_container_view,
                          overflow_button) {}
  MOCK_METHOD(bool, PopOut, (ui::ElementIdentifier identifier), (override));
  MOCK_METHOD(bool, EndPopOut, (ui::ElementIdentifier identifier), (override));
};

class PopOutHandlerTest : public views::ViewsTestBase {
 public:
  PopOutHandlerTest() = default;

  PopOutHandlerTest(const PopOutHandlerTest&) = delete;
  PopOutHandlerTest& operator=(const PopOutHandlerTest&) = delete;

  ~PopOutHandlerTest() override = default;

 protected:
  views::Widget* widget() { return widget_.get(); }
  views::View* container_view() { return container_view_; }
  views::View* overflow_button() { return overflow_button_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View, DanglingUntriaged> container_view_;
  raw_ptr<views::View, DanglingUntriaged> overflow_button_;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    container_view_ =
        widget()->SetContentsView(std::make_unique<views::View>());
    overflow_button_ =
        container_view_->AddChildView(std::make_unique<views::View>());
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }
};

TEST_F(PopOutHandlerTest, PopOutAndEndPopOut) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyObservedView);

  MockToolbarController toolbar_controller(
      {kDummyButton}, {{kDummyButton, kDummyObservedView}}, 1, container_view(),
      overflow_button());

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(widget());
  ToolbarController::PopOutHandler pop_out_controller(
      &toolbar_controller, context, kDummyButton, kDummyObservedView);

  EXPECT_CALL(toolbar_controller, PopOut(kDummyButton));
  auto observed_view = std::make_unique<views::View>();
  observed_view->SetProperty(views::kElementIdentifierKey, kDummyObservedView);
  views::View* view = container_view()->AddChildView(std::move(observed_view));

  EXPECT_CALL(toolbar_controller, EndPopOut(kDummyButton));
  container_view()->RemoveChildView(view);
}

constexpr int kElementFlexOrderStart = 1;

}  // namespace

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton3);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton4);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyObservedView);

class ToolbarControllerUnitTest : public ChromeViewsTestBase {
 public:
  ToolbarControllerUnitTest() = default;
  ToolbarControllerUnitTest(const ToolbarControllerUnitTest&) = delete;
  ToolbarControllerUnitTest& operator=(const ToolbarControllerUnitTest&) =
      delete;
  ~ToolbarControllerUnitTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    widget_->Show();

    std::unique_ptr<views::View> toolbar_container_view =
        std::make_unique<views::View>();
    toolbar_container_view
        ->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero,
                        views::MaximumFlexSizeRule::kPreferred));
    toolbar_container_view_ =
        widget_->SetContentsView(std::move(toolbar_container_view));

    std::vector<ui::ElementIdentifier> element_ids = {
        kDummyButton1, kDummyButton2, kDummyButton3};
    InitToolbarContainerViewWithTestButtons(element_ids);

    auto overflow_button = std::make_unique<OverflowButton>();
    overflow_button_ =
        toolbar_container_view_->AddChildView(std::move(overflow_button));
    overflow_button_->SetVisible(false);
    toolbar_controller_ = std::make_unique<ToolbarController>(
        element_ids,
        PopOutIdentifierMap{{kDummyButton1, kDummyObservedView},
                            {kDummyButton2, kDummyObservedView},
                            {kDummyButton3, kDummyObservedView}},
        kElementFlexOrderStart, toolbar_container_view_, overflow_button_);
    overflow_button_->set_create_menu_model_callback(
        base::BindRepeating(&ToolbarController::CreateOverflowMenuModel,
                            base::Unretained(toolbar_controller_.get())));

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget_.get()));
  }

  // Add test buttons with `ids` to `toolbar_container_view_`.
  void InitToolbarContainerViewWithTestButtons(
      std::vector<ui::ElementIdentifier> ids) {
    for (size_t i = 0; i < ids.size(); ++i) {
      auto button = std::make_unique<views::View>();
      button->SetProperty(views::kElementIdentifierKey, ids[i]);
      button->SetPreferredSize(kButtonSize);
      button->SetAccessibleName(
          base::StrCat({u"DummyButton", base::NumberToString16(i)}));
      button->SetVisible(true);
      test_buttons_.emplace_back(
          toolbar_container_view_->AddChildView(std::move(button)));
    }
  }

  void TearDown() override {
    overflow_button_ = nullptr;
    toolbar_container_view_ = nullptr;
    event_generator_.reset();
    toolbar_controller_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }
  ToolbarController* toolbar_controller() { return toolbar_controller_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  views::View* toolbar_container_view() { return toolbar_container_view_; }
  const views::View* overflow_button() { return overflow_button_; }
  const std::vector<views::View*>& test_buttons() { return test_buttons_; }
  const ui::SimpleMenuModel* overflow_menu() {
    return overflow_button_->menu_model_for_testing();
  }
  std::vector<const views::View*> GetOverflowedElements() {
    return toolbar_controller()->GetOverflowedElements();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ToolbarController> toolbar_controller_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  raw_ptr<views::View> toolbar_container_view_;
  raw_ptr<OverflowButton> overflow_button_;

  // Buttons being tested.
  std::vector<views::View*> test_buttons_;
};

TEST_F(ToolbarControllerUnitTest, OverflowButtonVisibility) {
  // Initialize widget width with total width of all test buttons.
  // Should not see overflowed buttons.
  widget()->SetSize(gfx::Size(kButtonSize.width() * test_buttons().size(),
                              kButtonSize.height()));
  EXPECT_EQ(GetOverflowedElements().size(), size_t(0));
  toolbar_controller()->SetOverflowButtonVisible(
      toolbar_controller()->ShouldShowOverflowButton());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Shrink widget width with one button width smaller.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  EXPECT_EQ(GetOverflowedElements().size(), size_t(1));
  toolbar_controller()->SetOverflowButtonVisible(
      toolbar_controller()->ShouldShowOverflowButton());
  EXPECT_TRUE(overflow_button()->GetVisible());
}

TEST_F(ToolbarControllerUnitTest, OverflowedButtonsMatchMenu) {
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  toolbar_controller()->SetOverflowButtonVisible(
      toolbar_controller()->ShouldShowOverflowButton());
  EXPECT_TRUE(overflow_button()->GetVisible());

  widget()->LayoutRootViewIfNecessary();
  event_generator()->MoveMouseTo(
      overflow_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();

  const ui::SimpleMenuModel* menu = overflow_menu();
  std::vector<const views::View*> overflowed_buttons = GetOverflowedElements();

  // Overflowed buttons should match overflow menu.
  EXPECT_TRUE(menu);
  EXPECT_EQ(overflowed_buttons.size(), menu->GetItemCount());
  for (size_t i = 0; i < overflowed_buttons.size(); ++i) {
    EXPECT_EQ(overflowed_buttons[i]->GetAccessibleName(), menu->GetLabelAt(i));
  }
}

TEST_F(ToolbarControllerUnitTest, PopOutState) {
  auto& pop_out_state = toolbar_controller()->pop_out_state_for_testing();
  EXPECT_FALSE(pop_out_state.at(kDummyButton1)->original_spec.has_value());
  EXPECT_FALSE(pop_out_state.at(kDummyButton2)->original_spec.has_value());
  EXPECT_FALSE(pop_out_state.at(kDummyButton3)->original_spec.has_value());
  EXPECT_EQ(pop_out_state.at(kDummyButton1)->responsive_spec.order(),
            kElementFlexOrderStart);
  EXPECT_EQ(pop_out_state.at(kDummyButton2)->responsive_spec.order(),
            kElementFlexOrderStart + 1);
  EXPECT_EQ(pop_out_state.at(kDummyButton3)->responsive_spec.order(),
            kElementFlexOrderStart + 2);
}

TEST_F(ToolbarControllerUnitTest, PopOutButton) {
  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Enough space to accommodate 3 buttons.
  widget()->SetSize(gfx::Size(kButtonSize.width() * test_buttons().size(),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Not enough space. Button3 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  // Pop out button3. Button2 is hidden.
  EXPECT_TRUE(toolbar_controller()->PopOut(kDummyButton3));
  views::test::RunScheduledLayout(toolbar_container_view());
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Button3 is already popped out.
  EXPECT_FALSE(toolbar_controller()->PopOut(kDummyButton3));

  // End button3 pop out. Button3 is hidden again.
  EXPECT_TRUE(toolbar_controller()->EndPopOut(kDummyButton3));
  views::test::RunScheduledLayout(toolbar_container_view());
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  // Button3 already ended popped out.
  EXPECT_FALSE(toolbar_controller()->EndPopOut(kDummyButton3));

  // kDummyButton4 does not exist.
  EXPECT_FALSE(toolbar_controller()->PopOut(kDummyButton4));
}
