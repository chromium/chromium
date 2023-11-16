// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

using ::testing::NiceMock;
using ::testing::Return;

namespace {

Suggestion CreatePasswordSuggestion(const std::u16string& main_text) {
  Suggestion suggestion(main_text, PopupItemId::kPasswordEntry);
  suggestion.icon = Suggestion::Icon::kKey;
  suggestion.additional_label = u"****";
  return suggestion;
}

// Suggestion main text (Suggestion::main_text) is used for the test and
// screenshot names, avoid special symbols and keep them unique.
const Suggestion kSuggestions[] = {
    Suggestion("Address_entry",
               "Minor text",
               "label",
               Suggestion::Icon::kLocation,
               PopupItemId::kAddressEntry),
    CreatePasswordSuggestion(u"Password_entry"),
    Suggestion("Autofill_options",
               "Minor text",
               "label",
               Suggestion::Icon::kSettings,
               PopupItemId::kAutofillOptions),
    Suggestion(u"Autocomplete", PopupItemId::kAutocompleteEntry),
    Suggestion("Compose",
               "Minor text",
               "label",
               Suggestion::Icon::kMagic,
               PopupItemId::kCompose),
    Suggestion("Edit_address",
               "label",
               Suggestion::Icon::kEdit,
               PopupItemId::kEditAddressProfile),
    Suggestion("Promo_code",
               "label",
               Suggestion::Icon::kGlobe,
               PopupItemId::kSeePromoCodeDetails),

};
}  // namespace

// TODO(crbug.com/1491373): Add tests for RTL and dark mode.
using TestParams =
    std::tuple<Suggestion, std::optional<PopupRowView::CellType>>;

class CreatePopupRowViewTest
    : public UiBrowserTest,
      public ::testing::WithParamInterface<TestParams> {
 protected:
  MockAutofillPopupController& controller() { return controller_; }

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();

    widget_ = CreateWidget();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ON_CALL(controller(), GetWebContents()).WillByDefault(Return(web_contents));
  }

  void TearDownOnMainThread() override {
    widget_.reset();

    UiBrowserTest::TearDownOnMainThread();
  }

  void CreateRowView(Suggestion suggestion,
                     std::optional<PopupRowView::CellType> selected_cell) {
    controller().set_suggestions({std::move(suggestion)});

    auto view = CreatePopupRowView(controller_.GetWeakPtr(),
                                   mock_a11y_selection_delegate_,
                                   mock_selection_delegate_, /*line_number=*/0);
    view->SetSelectedCell(selected_cell);

    widget_->SetContentsView(std::move(view));
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override { widget_->Show(); }

  bool VerifyUi() override {
    if (!widget_) {
      return false;
    }

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(widget_.get(), test_info->test_case_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 private:
  std::unique_ptr<views::Widget> CreateWidget() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    // Row view size depends on the parent view it's embedded into, 220x52 is
    // close to the actually used row size so that the screenshot is also close
    // to what is rendered in the popup.
    params.bounds = gfx::Rect(220, 52);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    widget->Init(std::move(params));
    return widget;
  }

  std::unique_ptr<views::Widget> widget_;
  NiceMock<MockAutofillPopupController> controller_;
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  base::test::ScopedFeatureList feature_list{
      features::kAutofillShowAutocompleteDeleteButton};
};

IN_PROC_BROWSER_TEST_P(CreatePopupRowViewTest, SuggestionRowUiTest) {
  CreateRowView(std::get<Suggestion>(GetParam()),
                std::get<std::optional<PopupRowView::CellType>>(GetParam()));
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CreatePopupRowViewTest,
    ::testing::Combine(::testing::ValuesIn(kSuggestions),
                       ::testing::ValuesIn({
                           std::optional<PopupRowView::CellType>(),
                           std::optional(PopupRowView::CellType::kContent),
                       })),
    [](const testing::TestParamInfo<TestParams>& info) {
      std::string suggestion_part =
          base::UTF16ToUTF8(std::get<Suggestion>(info.param).main_text.value);
      auto selection =
          std::get<std::optional<PopupRowView::CellType>>(info.param);
      std::string selection_part =
          !selection.has_value()                          ? "NotSelected"
          : selection == PopupRowView::CellType::kContent ? "ContentSelected"
                                                          : "ControlSelected";
      return suggestion_part + "_" + selection_part;
    });

}  // namespace autofill
