// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

namespace autofill {

namespace {

enum class StrategyType {
  kSuggestion,
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
  // The number of (non-separator) entries and the 1-indexed position of the
  // entry with `line_number` inside them.
  int set_size;
  int set_index;
};

const RowStrategyTestdata kTestcases[] = {
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kAddressEntry,
                           PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                           PopupItemId::kAutofillOptions},
        .line_number = 1,
        .strategy_type = StrategyType::kSuggestion,
        .set_size = 3,
        .set_index = 2,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kPasswordEntry,
                           PopupItemId::kAccountStoragePasswordEntry,
                           PopupItemId::kSeparator,
                           PopupItemId::kAllSavedPasswordsEntry},
        .line_number = 0,
        .strategy_type = StrategyType::kPasswordSuggestion,
        .set_size = 3,
        .set_index = 1,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kAddressEntry,
                           PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                           PopupItemId::kAutofillOptions},
        .line_number = 3,
        .strategy_type = StrategyType::kFooter,
        .set_size = 3,
        .set_index = 3,
    },
    RowStrategyTestdata{
        .popup_item_ids = {PopupItemId::kAutocompleteEntry,
                           PopupItemId::kAutocompleteEntry,
                           PopupItemId::kAutocompleteEntry},
        .line_number = 1,
        .strategy_type = StrategyType::kSuggestion,
        .set_size = 3,
        .set_index = 2,
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
      suggestions.emplace_back("Main text", "", "", popup_item_id);
    }
    controller().set_suggestions(std::move(suggestions));
  }

  // Checks that the expected callbacks for content cells are set and call the
  // controller.
  void TestContentCallbacks(const PopupCellView& cell, int index) {
    constexpr base::TimeTicks kTime = base::TimeTicks() + base::Seconds(5);
    PopupCellView::OnAcceptedCallback on_accept_callback =
        cell.GetOnAcceptedCallback();
    ASSERT_TRUE(on_accept_callback);
    EXPECT_CALL(controller(), AcceptSuggestion(index, kTime));
    on_accept_callback.Run(kTime);

    base::RepeatingClosure on_select_callback = cell.GetOnSelectedCallback();
    ASSERT_TRUE(on_select_callback);
    EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>(index)));
    on_select_callback.Run();

    base::RepeatingClosure on_unselect_callback =
        cell.GetOnUnselectedCallback();
    ASSERT_TRUE(on_unselect_callback);
    EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>()));
    on_unselect_callback.Run();
  }

  std::unique_ptr<PopupRowStrategy> CreateStrategy(StrategyType type,
                                                   int line_number) {
    switch (type) {
      case StrategyType::kSuggestion:
        return std::make_unique<PopupSuggestionStrategy>(
            controller().GetWeakPtr(), line_number);
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

TEST_P(PopupRowStrategyParametrizedTest, ContentAreaCallbacksWork) {
  const RowStrategyTestdata kTestdata = GetParam();

  SetSuggestions(kTestdata.popup_item_ids);
  std::unique_ptr<PopupRowStrategy> strategy =
      CreateStrategy(kTestdata.strategy_type, kTestdata.line_number);

  std::unique_ptr<PopupCellView> content_cell = strategy->CreateContent();
  ASSERT_THAT(content_cell, NotNull());
  TestContentCallbacks(*content_cell, kTestdata.line_number);
}

TEST_P(PopupRowStrategyParametrizedTest, DeletedControllerIsHandledGracefully) {
  const RowStrategyTestdata kTestdata = GetParam();

  SetSuggestions(kTestdata.popup_item_ids);
  std::unique_ptr<PopupRowStrategy> strategy =
      CreateStrategy(kTestdata.strategy_type, kTestdata.line_number);

  std::unique_ptr<PopupCellView> content_cell = strategy->CreateContent();
  ASSERT_THAT(content_cell, NotNull());

  // Test that the executing the callbacks does not crash even if the controller
  // has disappeared.
  PopupCellView::OnAcceptedCallback callback =
      content_cell->GetOnAcceptedCallback();
  controller().InvalidateWeakPtrs();
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  callback.Run(base::TimeTicks::Now());
}

TEST_P(PopupRowStrategyParametrizedTest,
       SetsAccessibilityAttributesForContentArea) {
  const RowStrategyTestdata kTestdata = GetParam();

  SetSuggestions(kTestdata.popup_item_ids);
  std::unique_ptr<PopupRowStrategy> strategy =
      CreateStrategy(kTestdata.strategy_type, kTestdata.line_number);

  std::unique_ptr<PopupCellView> content_cell = strategy->CreateContent();
  ASSERT_THAT(content_cell, NotNull());

  ui::AXNodeData node_data;
  content_cell->GetAccessibleNodeData(&node_data);

  EXPECT_EQ(node_data.role, ax::mojom::Role::kListBoxOption);
  EXPECT_EQ(u"Main text",
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            kTestdata.set_size);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            kTestdata.set_index);
}

TEST_P(PopupRowStrategyParametrizedTest, HasControlArea) {
  const RowStrategyTestdata kTestdata = GetParam();

  SetSuggestions(kTestdata.popup_item_ids);
  std::unique_ptr<PopupRowStrategy> strategy =
      CreateStrategy(kTestdata.strategy_type, kTestdata.line_number);

  EXPECT_THAT(strategy->CreateControl(), IsNull());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupRowStrategyParametrizedTest,
                         ::testing::ValuesIn(kTestcases));

}  // namespace autofill
