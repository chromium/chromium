// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"

#include <memory>
#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/password_favicon_loader.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/user_education/common/new_badge_controller.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

Suggestion CreatePasswordSuggestion(const std::u16string& main_text) {
  Suggestion suggestion(main_text, SuggestionType::kPasswordEntry);
  suggestion.icon = Suggestion::Icon::kKey;
  suggestion.labels = {{Suggestion::Text(u"****")}};
  return suggestion;
}

Suggestion CreateSuggestionWithChildren(const std::u16string& main_text,
                                        std::vector<Suggestion> children) {
  Suggestion suggestion(main_text, SuggestionType::kAddressEntry);
  suggestion.children = std::move(children);
  return suggestion;
}

// Suggestion main text (Suggestion::main_text) is used for the test and
// screenshot names, avoid special symbols and keep them unique.
const Suggestion kSuggestions[] = {
    Suggestion("Address_entry",
               "Minor text",
               "label",
               Suggestion::Icon::kLocation,
               SuggestionType::kAddressEntry),
    Suggestion("Fill_Full_Email_entry",
               "Minor text",
               "label",
               Suggestion::Icon::kNoIcon,
               SuggestionType::kFillFullEmail),
    CreatePasswordSuggestion(u"Password_entry"),
    Suggestion("Autofill_options",
               "Minor text",
               "label",
               Suggestion::Icon::kSettings,
               SuggestionType::kManageAddress),
    Suggestion(u"Autocomplete", SuggestionType::kAutocompleteEntry),
    Suggestion("Compose",
               "Minor text",
               "label",
               Suggestion::Icon::kMagic,
               SuggestionType::kComposeResumeNudge),
    Suggestion("Edit_address",
               "label",
               Suggestion::Icon::kEdit,
               SuggestionType::kEditAddressProfile),
    Suggestion("Promo_code",
               "label",
               Suggestion::Icon::kGlobe,
               SuggestionType::kSeePromoCodeDetails),
};
const Suggestion kExpandableSuggestions[] = {CreateSuggestionWithChildren(
    u"Address_entry",
    {Suggestion(u"Username", SuggestionType::kPasswordEntry)})};

class MockPasswordFaviconLoader : public PasswordFaviconLoader {
 public:
  MOCK_METHOD(void,
              Load,
              (const Suggestion::FaviconDetails&,
               base::CancelableTaskTracker*,
               OnLoadSuccess,
               OnLoadFail),
              (override));
};

// TODO(crbug.com/40285052): Add tests for RTL and dark mode.
using TestParams =
    std::tuple<Suggestion, std::optional<PopupRowView::CellType>>;

class BaseCreatePopupRowViewTest
    : public UiBrowserTest,
      public ::testing::WithParamInterface<TestParams> {
 public:
  BaseCreatePopupRowViewTest() = default;
  ~BaseCreatePopupRowViewTest() override = default;

  static std::string GetTestName(
      const testing::TestParamInfo<TestParams>& info) {
    const std::string suggestion_part =
        base::UTF16ToUTF8(std::get<Suggestion>(info.param).main_text.value);
    const auto selection =
        std::get<std::optional<PopupRowView::CellType>>(info.param);
    const std::string selection_part =
        !selection.has_value()                          ? "NotSelected"
        : selection == PopupRowView::CellType::kContent ? "ContentSelected"
                                                        : "ControlSelected";
    return suggestion_part + "_" + selection_part;
  }

 protected:
  MockAutofillPopupController& controller() { return controller_; }
  MockPasswordFaviconLoader& favicon_loader() { return favicon_loader_; }

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

  void CreateRowView(
      Suggestion suggestion,
      std::optional<PopupRowView::CellType> selected_cell,
      std::optional<AutofillPopupController::SuggestionFilterMatch>
          filter_match = std::nullopt) {
    controller().set_suggestions({std::move(suggestion)});

    auto view = CreatePopupRowView(controller_.GetWeakPtr(),
                                   mock_a11y_selection_delegate_,
                                   mock_selection_delegate_, /*line_number=*/0,
                                   std::move(filter_match), &favicon_loader());
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
    return VerifyPixelUi(widget_.get(), test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 private:
  std::unique_ptr<views::Widget> CreateWidget() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    // Row view size depends on the parent view it's embedded into, 220x52 is
    // close to the actually used row size so that the screenshot is also close
    // to what is rendered in the popup.
    params.bounds = gfx::Rect(220, 52);
    widget->Init(std::move(params));
    return widget;
  }

  std::unique_ptr<views::Widget> widget_;
  NiceMock<MockAutofillPopupController> controller_;
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  NiceMock<MockPasswordFaviconLoader> favicon_loader_;
};

class CreatePopupRowViewTest : public BaseCreatePopupRowViewTest {
 public:
  CreatePopupRowViewTest() = default;
  ~CreatePopupRowViewTest() override = default;

 private:
  user_education::NewBadgeController::TestLock disable_new_badges_ =
      user_education::NewBadgeController::DisableNewBadgesForTesting();
};

IN_PROC_BROWSER_TEST_P(CreatePopupRowViewTest, SuggestionRowUiTest) {
  CreateRowView(std::get<Suggestion>(GetParam()),
                std::get<std::optional<PopupRowView::CellType>>(GetParam()));
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    Suggestions,
    CreatePopupRowViewTest,
    ::testing::Combine(::testing::ValuesIn(kSuggestions),
                       ::testing::ValuesIn({
                           std::optional<PopupRowView::CellType>(),
                           std::optional(PopupRowView::CellType::kContent),
                       })),
    CreatePopupRowViewTest::GetTestName);

INSTANTIATE_TEST_SUITE_P(
    ExpandableSuggestions,
    CreatePopupRowViewTest,
    ::testing::Combine(::testing::ValuesIn(kExpandableSuggestions),
                       ::testing::ValuesIn({
                           std::optional<PopupRowView::CellType>(),
                           std::optional(PopupRowView::CellType::kContent),
                           std::optional(PopupRowView::CellType::kControl),
                       })),
    CreatePopupRowViewTest::GetTestName);

IN_PROC_BROWSER_TEST_F(CreatePopupRowViewTest, FilterMatchHighlighting) {
  CreateRowView(
      Suggestion("Address_entry", "Minor text", "label",
                 Suggestion::Icon::kLocation, SuggestionType::kAddressEntry),
      /*selected_cell=*/std::nullopt,
      AutofillPopupController::SuggestionFilterMatch{.main_text_match =
                                                         gfx::Range(1, 5)});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CreatePopupRowViewTest, PasswordWithFaviconPlaceholder) {
  Suggestion suggestion = CreatePasswordSuggestion(u"Password_entry");
  suggestion.custom_icon =
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"));
  CreateRowView(std::move(suggestion), /*selected_cell=*/std::nullopt,
                /*filter_match=*/std::nullopt);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CreatePopupRowViewTest, PasswordCustomIconLoader) {
  ON_CALL(favicon_loader(), Load)
      .WillByDefault([](const Suggestion::FaviconDetails&,
                        base::CancelableTaskTracker* task_tracker,
                        PasswordFaviconLoader::OnLoadSuccess on_success,
                        PasswordFaviconLoader::OnLoadFail on_fail) {
        std::move(on_success)
            .Run(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                IDR_DISABLE));
      });

  Suggestion suggestion("Password_entry", "Minor text", "label",
                        Suggestion::Icon::kKey, SuggestionType::kPasswordEntry);
  suggestion.custom_icon =
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"));
  CreateRowView(std::move(suggestion),
                /*selected_cell=*/std::nullopt, /*filter_match=*/std::nullopt);
  ShowAndVerifyUi();
}

class CreatePopupRowViewWithNoUserEducationRateLimitTest
    : public BaseCreatePopupRowViewTest {
 public:
  CreatePopupRowViewWithNoUserEducationRateLimitTest() = default;
  ~CreatePopupRowViewWithNoUserEducationRateLimitTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        user_education::features::kDisableRateLimitingCommandLine);
  }
};

IN_PROC_BROWSER_TEST_F(CreatePopupRowViewWithNoUserEducationRateLimitTest,
                       ComposeWithNewBadge) {
  Suggestion suggestion("Compose with a badge", "Minor text", "label",
                        Suggestion::Icon::kMagic,
                        SuggestionType::kComposeProactiveNudge);
  suggestion.feature_for_new_badge =
      &compose::features::kEnableComposeProactiveNudge;

  CreateRowView(std::move(suggestion), /*selected_cell=*/std::nullopt,
                /*filter_match=*/std::nullopt);
  ShowAndVerifyUi();
}

}  // namespace
}  // namespace autofill
