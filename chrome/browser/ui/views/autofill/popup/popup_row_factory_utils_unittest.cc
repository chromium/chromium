// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"

using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

namespace autofill {

class PopupRowFactoryUtilsTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    row_view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void ShowSuggestion(Suggestion suggestion) {
    // Show the button.
    controller().set_suggestions({std::move(suggestion)});
    row_view_ = widget().SetContentsView(
        CreatePopupRowView(controller().GetWeakPtr(), a11y_selection_delegate(),
                           selection_delegate(), 0));
    widget().Show();
  }

  MockAutofillPopupController& controller() { return controller_; }
  MockAccessibilitySelectionDelegate& a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  MockSelectionDelegate& selection_delegate() {
    return mock_selection_delegate_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupRowView& row_view() { return *row_view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  MockAutofillPopupController controller_;
  MockAccessibilitySelectionDelegate mock_a11y_selection_delegate_;
  MockSelectionDelegate mock_selection_delegate_;
  raw_ptr<PopupRowView> row_view_ = nullptr;
};

// A test fixture for testing the creation of rows that contain a button.
class PopupRowFactoryUtilsRowWithButtonTest : public PopupRowFactoryUtilsTest {
 public:

  void ShowAutocompleteSuggestion() {
    ShowSuggestion(
        Suggestion(u"Some entry", SuggestionType::kAutocompleteEntry));
  }

  void ShowCreateNewPlusAddressInlineSuggestion() {
    Suggestion suggestion(u"Create new plus address",
                          SuggestionType::kCreateNewPlusAddressInline);
    suggestion.payload = Suggestion::PlusAddressPayload();
    ShowSuggestion(suggestion);
  }

 protected:
  PopupRowWithButtonView& row_with_button_view() {
    CHECK(views::IsViewClass<PopupRowWithButtonView>(&row_view()));
    return static_cast<PopupRowWithButtonView&>(row_view());
  }
};

// Tests that if `offer_refresh` is set to `false` in the payload, then a plus
// address error suggestion does not have a refresh button.
TEST_F(PopupRowFactoryUtilsTest, PlusAddressErrorSuggestionWithoutRefresh) {
  Suggestion suggestion(u"Create new plus address",
                        SuggestionType::kPlusAddressError);
  Suggestion::PlusAddressPayload payload;
  payload.offer_refresh = false;
  suggestion.payload = std::move(payload);
  ShowSuggestion(suggestion);

  EXPECT_FALSE(views::IsViewClass<PopupRowWithButtonView>(&row_view()));
}

TEST_F(PopupRowFactoryUtilsRowWithButtonTest,
       AutocompleteDeleteInvokesController) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = row_with_button_view().GetButtonForTest();
  row_with_button_view().SetSelectedCell(PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          0, AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(Return(true));

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
  task_environment()->RunUntilIdle();
}

TEST_F(PopupRowFactoryUtilsRowWithButtonTest,
       AutocompleteDeleteButtonHasTooltip) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = row_with_button_view().GetButtonForTest();
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
}

TEST_F(PopupRowFactoryUtilsRowWithButtonTest,
       AutocompleteDeleteButtonSetsAccessibility) {
  ShowAutocompleteSuggestion();
  views::ImageButton* button = row_with_button_view().GetButtonForTest();

  views::IgnoreMissingWidgetForTestingScopedSetter ignore_missing_widget(
      button->GetViewAccessibility());
  ui::AXNodeData node_data;
  button->GetViewAccessibility().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(node_data.role, ax::mojom::Role::kMenuItem);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT, u"Some entry"),
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PopupRowFactoryUtilsRowWithButtonTest,
       CreateNewPlusAddressInlineSuggestionRefreshInvokesController) {
  ShowCreateNewPlusAddressInlineSuggestion();
  views::ImageButton* button = row_with_button_view().GetButtonForTest();
  row_with_button_view().SetSelectedCell(PopupRowView::CellType::kContent);
  // In test env we have to manually set the bounds when a view becomes visible.
  button->parent()->SetBoundsRect(gfx::Rect(0, 0, 30, 30));

  base::RunLoop loop;
  base::RepeatingClosure quit_closure = loop.QuitClosure();
  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(0, SuggestionButtonAction()))
      .WillOnce([&quit_closure] { std::move(quit_closure).Run(); });

  generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
  loop.Run();
}

TEST_F(PopupRowFactoryUtilsRowWithButtonTest,
       CreateNewPlusAddressInlineSuggestionHasTooltip) {
  ShowCreateNewPlusAddressInlineSuggestion();
  views::ImageButton* button = row_with_button_view().GetButtonForTest();
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_CREATE_INLINE_REFRESH_TOOLTIP));
}

TEST_F(PopupRowFactoryUtilsRowWithButtonTest,
       CreateNewPlusAddressInlineSuggestionSetsAccessibility) {
  ShowCreateNewPlusAddressInlineSuggestion();
  views::ImageButton* button = row_with_button_view().GetButtonForTest();

  views::IgnoreMissingWidgetForTestingScopedSetter ignore_missing_widget(
      button->GetViewAccessibility());
  ui::AXNodeData node_data;
  button->GetViewAccessibility().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(node_data.role, ax::mojom::Role::kMenuItem);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_CREATE_INLINE_REFRESH_A11Y_NAME),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

class PasswordPopupRowViewTest : public PopupRowFactoryUtilsTest {
 public:
  void ShowPasswordSuggestionWithLoadingState(bool is_loading) {
    Suggestion suggestion(u"ortiler", SuggestionType::kPasswordEntry);
    suggestion.labels = {{Suggestion::Text(u"password")}};
    suggestion.is_loading = Suggestion::IsLoading(is_loading);
    ShowSuggestion(suggestion);
  }
};

TEST_F(PasswordPopupRowViewTest, LoadingSuggestionShowsThrobber) {
  ShowPasswordSuggestionWithLoadingState(true);

  EXPECT_TRUE(views::IsViewClass<views::Throbber>(
      row_view().GetContentView().children().at(0)));
}

TEST_F(PasswordPopupRowViewTest, NonLoadingSuggestionDoesNotShowThrobber) {
  ShowPasswordSuggestionWithLoadingState(false);

  EXPECT_FALSE(views::IsViewClass<views::Throbber>(
      row_view().GetContentView().children().at(0)));
}

}  // namespace autofill
