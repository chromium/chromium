// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_with_button_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/test_popup_row_strategy.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/input/native_web_keyboard_event.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

using ::testing::Return;
using ::testing::StrictMock;
using CellButtonBehavior = PopupCellWithButtonView::CellButtonBehavior;

class PopupCellWithButtonViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void ShowView(std::unique_ptr<PopupCellWithButtonView> cell_view) {
    view_ = widget_->SetContentsView(std::move(cell_view));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SimulateKeyPress(int windows_key_code) {
    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    view().HandleKeyPressEvent(event);
  }

  views::ImageButton* CreateRowAndGetButton(
      base::RepeatingClosure button_callback = base::DoNothing(),
      CellButtonBehavior cell_button_behavior =
          CellButtonBehavior::kShowOnHoverOrSelect) {
    auto cell = std::make_unique<PopupCellWithButtonView>(
        controller_.GetWeakPtr(), /*line_number=*/0);
    cell->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    cell->AddChildView(std::make_unique<views::Label>(u"Some label"));
    cell->SetAccessibilityDelegate(
        std::make_unique<TestAccessibilityDelegate>());
    cell->SetCellButtonBehavior(cell_button_behavior);
    cell->SetCellButton(
        std::make_unique<views::ImageButton>(std::move(button_callback)));
    views::ImageButton* button = cell->GetCellButtonForTest();
    ShowView(std::move(cell));
    return button;
  }

 protected:
  MockAutofillPopupController& controller() { return controller_; }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupCellWithButtonView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  MockAutofillPopupController controller_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupCellWithButtonView> view_ = nullptr;
};

TEST_F(PopupCellWithButtonViewTest, ShowsOrHideButtonOnSelected) {
  views::ImageButton* button = CreateRowAndGetButton();

  // The button is shown if the row is selected.
  EXPECT_FALSE(button->GetVisible());
  view().SetSelected(true);
  EXPECT_TRUE(button->GetVisible());

  // The button is hidden if the row is not selected.
  view().SetSelected(false);
  EXPECT_FALSE(button->GetVisible());
}

// Tests that the cell button is always visible for
// `CellButtonBehavior::kShowAlways`.
TEST_F(PopupCellWithButtonViewTest, DoNotHideButtonForShowAlwaysBehavior) {
  views::ImageButton* button = CreateRowAndGetButton(
      /*button_callback=*/base::DoNothing(), CellButtonBehavior::kShowAlways);

  EXPECT_TRUE(button->GetVisible());
  view().SetSelected(true);
  EXPECT_TRUE(button->GetVisible());

  view().SetSelected(false);
  EXPECT_TRUE(button->GetVisible());
}

TEST_F(PopupCellWithButtonViewTest,
       UpdateSelectedAndRunCallbacksOnButtonHovered) {
  views::ImageButton* button = CreateRowAndGetButton();
  views::View* label = view().children()[0];

  view().SetSelected(true);
  ASSERT_TRUE(view().GetSelected());
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  // Selected is false if hovering button.
  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>()));
  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  ASSERT_FALSE(view().GetSelected());

  // Selected is true if hovering the label when the button state changes.
  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>(0)));
  generator().MoveMouseTo(label->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(view().GetSelected());
}

TEST_F(PopupCellWithButtonViewTest, ButtonClickTriggersCallback) {
  base::MockCallback<base::RepeatingClosure> callback;
  views::ImageButton* button = CreateRowAndGetButton(callback.Get());

  view().SetSelected(true);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));
  EXPECT_CALL(callback, Run);

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupCellWithButtonViewTest, KeyPressLeftRightEnter) {
  base::MockCallback<base::RepeatingClosure> callback;
  CreateRowAndGetButton(callback.Get());

  view().SetSelected(true);
  // Pressing left does nothing.
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_TRUE(view().GetSelected());

  // Pressing right unselects and highlights the button.
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_FALSE(view().GetSelected());

  // Pressing enter when the button is focused triggers the callback.
  EXPECT_CALL(callback, Run);
  SimulateKeyPress(ui::VKEY_RETURN);
}

TEST_F(PopupCellWithButtonViewTest, CursorVerticalNavigationAlwaysHidesButton) {
  views::ImageButton* button = CreateRowAndGetButton();
  view().SetSelected(true);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));
  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(button->GetVisible());

  // Pressing down to indicate vertical navigation.
  SimulateKeyPress(ui::VKEY_DOWN);
  // Set selected as false to simulate another row was selected.
  view().SetSelected(false);

  ASSERT_FALSE(button->GetVisible());
}

}  // namespace autofill
