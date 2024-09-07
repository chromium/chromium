// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ButtonVisibility = PopupRowWithButtonView::ButtonVisibility;
using ButtonSelectBehavior = PopupRowWithButtonView::ButtonSelectBehavior;

class PopupRowWithButtonViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions({SuggestionType::kAddressEntry});
  }

  void ShowView(std::unique_ptr<PopupRowWithButtonView> view) {
    view_ = widget_->SetContentsView(std::move(view));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SimulateKeyPress(int windows_key_code) {
    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    view().HandleKeyPressEvent(event);
  }

  views::ImageButton* CreateRowAndGetButton(
      base::RepeatingClosure button_callback = base::DoNothing(),
      ButtonVisibility button_visibility =
          ButtonVisibility::kShowOnHoverOrSelect,
      ButtonSelectBehavior select_behavior =
          ButtonSelectBehavior::kUnselectSuggestion) {
    auto content_view = std::make_unique<PopupRowContentView>();
    content_view->AddChildView(std::make_unique<views::Label>(u"Some label"));
    auto row = std::make_unique<PopupRowWithButtonView>(
        a11y_selection_delegate(), selection_delegate(),
        controller_.GetWeakPtr(), 0, std::move(content_view),
        std::make_unique<views::ImageButton>(std::move(button_callback)),
        button_visibility, select_behavior);
    views::ImageButton* button = row->GetButtonForTest();
    ShowView(std::move(row));
    return button;
  }

 protected:
  MockAutofillPopupController& controller() { return controller_; }
  MockAccessibilitySelectionDelegate& a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  MockSelectionDelegate& selection_delegate() {
    return mock_selection_delegate_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupRowWithButtonView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  NiceMock<MockAutofillPopupController> controller_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupRowWithButtonView> view_ = nullptr;
};

TEST_F(PopupRowWithButtonViewTest, ShowsOrHideButtonOnSelected) {
  views::ImageButton* button = CreateRowAndGetButton();

  // The button is shown if the row is selected.
  EXPECT_FALSE(button->GetVisible());
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_TRUE(button->GetVisible());

  // The button is hidden if the row is not selected.
  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(button->GetVisible());
}

// Tests that the cell button is always visible for
// `ButtonVisibility::kShowAlways`.
TEST_F(PopupRowWithButtonViewTest, DoNotHideButtonForShowAlwaysBehavior) {
  views::ImageButton* button = CreateRowAndGetButton(
      /*button_callback=*/base::DoNothing(), ButtonVisibility::kShowAlways);

  EXPECT_TRUE(button->GetVisible());
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_TRUE(button->GetVisible());

  view().SetSelectedCell(std::nullopt);
  EXPECT_TRUE(button->GetVisible());
}

// Tests that when the select behavior is kSelectSuggestion, hovering the button
// does not unselect the suggestion.
TEST_F(PopupRowWithButtonViewTest, HoverWithSelectBehaviorSelectSuggestion) {
  views::ImageButton* button =
      CreateRowAndGetButton(/*button_callback=*/base::DoNothing(),
                            ButtonVisibility::kShowOnHoverOrSelect,
                            ButtonSelectBehavior::kSelectSuggestion);
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(view().GetSelectedCell(), PopupRowView::CellType::kContent);
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  EXPECT_CALL(controller(), UnselectSuggestion).Times(0);
  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(view().GetButtonFocusedForTest());
}

TEST_F(PopupRowWithButtonViewTest,
       UpdateFocusedPartAndRunCallbacksOnButtonHovered) {
  views::ImageButton* button = CreateRowAndGetButton();
  views::View* label = view().children()[0];

  view().SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(view().GetSelectedCell(), PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  // The button becomes focused if it is hovered.
  EXPECT_CALL(controller(), UnselectSuggestion);
  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(view().GetButtonFocusedForTest());

  // Selected is true if hovering the label when the button state changes.
  EXPECT_CALL(controller(), SelectSuggestion(0u));
  generator().MoveMouseTo(label->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(view().GetButtonFocusedForTest());
}

TEST_F(PopupRowWithButtonViewTest, ButtonClickTriggersCallback) {
  base::MockCallback<base::RepeatingClosure> callback;
  views::ImageButton* button = CreateRowAndGetButton(callback.Get());

  view().SetSelectedCell(PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));
  EXPECT_CALL(callback, Run);

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupRowWithButtonViewTest, KeyPressLeftRightEnter) {
  base::MockCallback<base::RepeatingClosure> callback;
  CreateRowAndGetButton(callback.Get());

  view().SetSelectedCell(PopupRowView::CellType::kContent);
  // Pressing left does nothing.
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(view().GetSelectedCell(), PopupRowView::CellType::kContent);
  EXPECT_FALSE(view().GetButtonFocusedForTest());

  // Pressing right unselects and highlights the button.
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_TRUE(view().GetButtonFocusedForTest());

  // Pressing enter when the button is focused triggers the callback.
  EXPECT_CALL(callback, Run);
  SimulateKeyPress(ui::VKEY_RETURN);
}

TEST_F(PopupRowWithButtonViewTest, CursorVerticalNavigationAlwaysHidesButton) {
  views::ImageButton* button = CreateRowAndGetButton();
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));
  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(button->GetVisible());

  // Pressing down to indicate vertical navigation.
  SimulateKeyPress(ui::VKEY_DOWN);
  // Set selected as false to simulate another row was selected.
  view().SetSelectedCell(std::nullopt);

  ASSERT_FALSE(button->GetVisible());
}

TEST_F(PopupRowWithButtonViewTest, AutocompleteControlsFocusByKeyboardKeys) {
  CreateRowAndGetButton();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_TRUE(view().GetButtonFocusedForTest());

  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_FALSE(view().GetButtonFocusedForTest());
}

}  // namespace autofill
