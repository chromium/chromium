// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget_utils.h"

using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

namespace autofill {

namespace {

enum class StrategyType {
  kSuggestion,
  kComposeSuggestion,
  kPasswordSuggestion,
  kFooter,
};

struct RowStrategyTestdata {
  // The popup item ids of the suggestions to be shown.
  std::vector<PopupItemId> popup_item_ids;
  // The index of the suggestion to be tested.
  int line_number;
  // The type of strategy to be tested.
  StrategyType strategy_type;
};

const RowStrategyTestdata kTestcases[] = {
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kAddressEntry,
                           PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                           PopupItemId::kAutofillOptions},
        .line_number = 1,
        .strategy_type = StrategyType::kSuggestion,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kPasswordEntry,
                           PopupItemId::kAccountStoragePasswordEntry,
                           PopupItemId::kSeparator,
                           PopupItemId::kAllSavedPasswordsEntry},
        .line_number = 0,
        .strategy_type = StrategyType::kPasswordSuggestion,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kAddressEntry,
                           PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                           PopupItemId::kAutofillOptions},
        .line_number = 3,
        .strategy_type = StrategyType::kFooter,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kAutocompleteEntry,
                           PopupItemId::kAutocompleteEntry,
                           PopupItemId::kAutocompleteEntry},
        .line_number = 1,
        .strategy_type = StrategyType::kSuggestion,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kCompose},
        .line_number = 0,
        .strategy_type = StrategyType::kComposeSuggestion,
    }};

}  // namespace

// Test fixture for testing PopupRowStrategy. Note that most of the detailed
// view testing is covered by pixel tests in `popup_view_views_browsertest.cc`.
class PopupRowStrategyTest : public ChromeViewsTestBase {
 public:
  // Sets suggestions in the mocked popup controller.
  void SetSuggestions(const std::vector<PopupItemId>& popup_item_ids) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(popup_item_ids.size());
    for (PopupItemId popup_item_id : popup_item_ids) {
      // Create a suggestion with empty labels.
      suggestions.emplace_back("Main text", "", Suggestion::Icon::kNoIcon,
                               popup_item_id);
    }
    controller().set_suggestions(std::move(suggestions));
  }

  std::unique_ptr<PopupRowStrategy> CreateStrategy(StrategyType type,
                                                   int line_number) {
    switch (type) {
      case StrategyType::kSuggestion:
        return std::make_unique<PopupSuggestionStrategy>(
            controller().GetWeakPtr(), line_number);
      case StrategyType::kComposeSuggestion:
        return std::make_unique<PopupComposeSuggestionStrategy>(
            controller().GetWeakPtr(), line_number, /*show_new_badge=*/false);
      case StrategyType::kPasswordSuggestion:
        return std::make_unique<PopupPasswordSuggestionStrategy>(
            controller().GetWeakPtr(), line_number);
      case StrategyType::kFooter:
        return std::make_unique<PopupFooterStrategy>(controller().GetWeakPtr(),
                                                     line_number);
    }
  }

  MockAutofillPopupController& controller() { return controller_; }

 private:
  MockAutofillPopupController controller_;
};

class PopupRowStrategyParametrizedTest
    : public PopupRowStrategyTest,
      public ::testing::WithParamInterface<RowStrategyTestdata> {};

TEST_P(PopupRowStrategyParametrizedTest, HasContentArea) {
  const RowStrategyTestdata kTestdata = GetParam();

  SetSuggestions(kTestdata.popup_item_ids);
  std::unique_ptr<PopupRowStrategy> strategy =
      CreateStrategy(kTestdata.strategy_type, kTestdata.line_number);

  // Every suggestion has a content area.
  EXPECT_THAT(strategy->CreateContent(), NotNull());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupRowStrategyParametrizedTest,
                         ::testing::ValuesIn(kTestcases));

class PopupSuggestionStrategyTest : public ChromeViewsTestBase {
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

  void ShowSuggestion(Suggestion suggestion) {
    // Show the button.
    controller().set_suggestions({std::move(suggestion)});
    strategy_ = std::make_unique<PopupSuggestionStrategy>(
        controller().GetWeakPtr(), /*line_number=*/0);
    view_ = widget_->SetContentsView(strategy_->CreateContent());
    widget_->Show();
  }

  void ShowAutocompleteSuggestion() {
    ShowSuggestion(Suggestion(u"Some entry", PopupItemId::kAutocompleteEntry));
  }

 protected:
  MockAutofillPopupController& controller() { return controller_; }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupCellView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupCellView> view_ = nullptr;
  MockAutofillPopupController controller_;
  std::unique_ptr<PopupSuggestionStrategy> strategy_;
  // All current Autocomplete tests assume that the deletion button feature is
  // enabled.
  base::test::ScopedFeatureList feature_list{
      features::kAutofillShowAutocompleteDeleteButton};
};

}  // namespace autofill
