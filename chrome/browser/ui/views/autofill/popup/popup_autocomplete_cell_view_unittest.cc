// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_autocomplete_cell_view.h"

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
#include "content/public/browser/native_web_keyboard_event.h"
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

using ::testing::Return;
using testing::StrictMock;

namespace autofill {

class PopupAutocompleteCellViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void ShowView(std::unique_ptr<PopupAutocompleteCellView> cell_view) {
    view_ = widget_->SetContentsView(std::move(cell_view));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SimulateKeyPress(int windows_key_code,
                        bool shift_modifier_pressed = false,
                        bool non_shift_modifier_pressed = false) {
    int modifiers = blink::WebInputEvent::kNoModifiers;
    if (shift_modifier_pressed) {
      modifiers |= blink::WebInputEvent::Modifiers::kShiftKey;
    }
    if (non_shift_modifier_pressed) {
      modifiers |= blink::WebInputEvent::Modifiers::kAltKey;
    }

    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown, modifiers,
        ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    view().HandleKeyPressEvent(event);
  }

  views::ImageButton* CreateRowAndGetDeleteButton() {
    controller().set_suggestions(
        {Suggestion("Jane Doe", "", "", "", PopupItemId::kAutocompleteEntry)});
    std::unique_ptr<PopupAutocompleteCellView> cell =
        std::make_unique<PopupAutocompleteCellView>(controller().GetWeakPtr(),
                                                    /*line_number=*/0);
    cell->SetAccessibilityDelegate(
        std::make_unique<TestAccessibilityDelegate>());
    views::ImageButton* button = cell->GetDeleteButton();
    ShowView(std::move(cell));
    return button;
  }

  MockAutofillPopupController& controller() { return controller_; }

 private:
  MockAutofillPopupController controller_;

 protected:
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupAutocompleteCellView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupAutocompleteCellView> view_ = nullptr;
};

TEST_F(PopupAutocompleteCellViewTest, ShowsOrHideDeleteIconOnSelected) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();

  // Show delete button if row is selected.
  ASSERT_FALSE(button->GetVisible());
  view().SetSelected(true);
  ASSERT_TRUE(button->GetVisible());

  // Hide delete button if row is not selected.
  view().SetSelected(false);
  ASSERT_FALSE(button->GetVisible());
}

TEST_F(PopupAutocompleteCellViewTest,
       UpdateSelectedAndRunCallbacksOnButtonHovered) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();
  views::View* table = view().children()[0];

  StrictMock<base::MockCallback<base::RepeatingClosure>> selected_callback;
  StrictMock<base::MockCallback<base::RepeatingClosure>> unselected_callback;
  view().SetOnSelectedCallback(selected_callback.Get());
  view().SetOnUnselectedCallback(unselected_callback.Get());
  EXPECT_CALL(selected_callback, Run);
  view().SetSelected(true);
  ASSERT_TRUE(view().GetSelected());
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  // Selected is false if hovering button.
  EXPECT_CALL(unselected_callback, Run);
  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  ASSERT_FALSE(view().GetSelected());

  // Selected is true if hovering label when button state changes
  EXPECT_CALL(selected_callback, Run);
  generator().MoveMouseTo(table->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(view().GetSelected());
}

TEST_F(PopupAutocompleteCellViewTest, AutocompleteDeleteButtonRemovesEntry) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();
  view().SetSelected(true);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));
  EXPECT_CALL(controller(), RemoveSuggestion(0));

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupAutocompleteCellViewTest,
       AutocompleteDeleteRecordsMetricOnDeletion) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();
  base::HistogramTester histogram_tester;

  view().SetSelected(true);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  EXPECT_CALL(controller(), RemoveSuggestion(0)).WillOnce(Return(true));

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
          kDeleteButtonClicked,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 1);
}

TEST_F(PopupAutocompleteCellViewTest,
       AutocompleteDeleteRecordsNoMetricOnFailedDeletion) {
  base::HistogramTester histogram_tester;
  views::ImageButton* button = CreateRowAndGetDeleteButton();
  view().SetSelected(true);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  EXPECT_CALL(controller(), RemoveSuggestion(0)).WillOnce(Return(false));

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
          kDeleteButtonClicked,
      0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(PopupAutocompleteCellViewTest, KeyPressLeftRightEnter) {
  CreateRowAndGetDeleteButton();
  view().SetSelected(true);

  // Pressing left does nothing.
  SimulateKeyPress(ui::VKEY_LEFT);
  ASSERT_TRUE(view().GetSelected());

  // Pressing right unselects and highlights the button.
  SimulateKeyPress(ui::VKEY_RIGHT);
  ASSERT_FALSE(view().GetSelected());

  // Pressing enter when the button is focused deletes the entry.
  EXPECT_CALL(controller(), RemoveSuggestion(0));
  SimulateKeyPress(ui::VKEY_RETURN);
}

TEST_F(PopupAutocompleteCellViewTest,
       CursorVerticalNavigationAlwaysHidesButton) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();
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

TEST_F(PopupAutocompleteCellViewTest, AutocompleteDeleteButtonHasTooltip) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();

  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
}

TEST_F(PopupAutocompleteCellViewTest,
       AutocompleteDeleteButtonSetsAccessibility) {
  views::ImageButton* button = CreateRowAndGetDeleteButton();
  // We only set the accessible name once user navigates to the button.
  // TODO(crbug.com/1417187): Delete this once we find out why calling
  // NotifyAccessibilityEvent in the content is including the button's
  // accessible name attribute value.
  SimulateKeyPress(ui::VKEY_RIGHT);
  ui::AXNodeData node_data;
  button->GetAccessibleNodeData(&node_data);

  EXPECT_EQ(node_data.role, ax::mojom::Role::kMenuItem);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT, u"Jane Doe"),
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

}  // namespace autofill
