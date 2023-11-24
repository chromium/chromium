// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "base/check_op.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_utils.h"

using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

namespace autofill {

class AutocompleteRowWithDeleteButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
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

  void ShowSuggestion(Suggestion suggestion) {
    // Show the button.
    controller().set_suggestions({std::move(suggestion)});
    PopupRowView* view = widget_->SetContentsView(
        CreatePopupRowView(controller().GetWeakPtr(), a11y_selection_delegate(),
                           selection_delegate(), 0));
    CHECK_EQ(view->GetClassMetaData()->type_name(), "PopupRowWithButtonView");
    view_ = static_cast<PopupRowWithButtonView*>(view);
    widget_->Show();
  }

  void ShowAutocompleteSuggestion() {
    ShowSuggestion(Suggestion(u"Some entry", PopupItemId::kAutocompleteEntry));
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
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupRowWithButtonView> view_ = nullptr;
  MockAutofillPopupController controller_;
  MockAccessibilitySelectionDelegate mock_a11y_selection_delegate_;
  MockSelectionDelegate mock_selection_delegate_;

  // All current Autocomplete tests assume that the deletion button feature is
  // enabled.
  base::test::ScopedFeatureList feature_list{
      features::kAutofillShowAutocompleteDeleteButton};
};

TEST_F(AutocompleteRowWithDeleteButtonTest,
       AutocompleteDeleteRecordsMetricOnDeletion) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = view().GetButtonForTest();
  base::HistogramTester histogram_tester;
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  EXPECT_CALL(controller(), RemoveSuggestion(0)).WillOnce(Return(true));

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
  task_environment()->RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
          kDeleteButtonClicked,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 1);
}

TEST_F(AutocompleteRowWithDeleteButtonTest,
       AutocompleteDeleteRecordsNoMetricOnFailedDeletion) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = view().GetButtonForTest();
  base::HistogramTester histogram_tester;
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  EXPECT_CALL(controller(), RemoveSuggestion(0)).WillOnce(Return(false));

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
  task_environment()->RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
          kDeleteButtonClicked,
      0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutocompleteRowWithDeleteButtonTest,
       AutocompleteDeleteButtonHasTooltip) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = view().GetButtonForTest();
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
}

TEST_F(AutocompleteRowWithDeleteButtonTest,
       AutocompleteDeleteButtonSetsAccessibility) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = view().GetButtonForTest();
  // We only set the accessible name once the user navigates to the button.
  // TODO(crbug.com/1417187): Delete this once we find out why calling
  // NotifyAccessibilityEvent in the content is including the button's
  // accessible name attribute value.
  SimulateKeyPress(ui::VKEY_RIGHT);
  ui::AXNodeData node_data;
  button->GetAccessibleNodeData(&node_data);

  EXPECT_EQ(node_data.role, ax::mojom::Role::kMenuItem);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT, u"Some entry"),
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

}  // namespace autofill
