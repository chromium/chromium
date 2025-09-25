// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"

#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform {

class ContextualPageActionsModelTest : public DefaultModelTestBase {
 public:
  ContextualPageActionsModelTest()
      : DefaultModelTestBase(std::make_unique<ContextualPageActionsModel>()) {}
  ~ContextualPageActionsModelTest() override = default;

  ModelProvider::Request GetRequestInput(
      bool has_price_insights,
      bool has_price_tracking,
      bool has_reader_mode,
      bool has_discounts = false,
      bool has_tab_grouping_suggestions = false,
      float non_contextual_click_count = 0,
      float tab_group_shown_count = 0,
      float tab_group_clicked_count = 0) {
    ModelProvider::Request input;
    input.push_back(has_discounts ? 1 : 0);
    input.push_back(has_price_insights ? 1 : 0);
    input.push_back(has_price_tracking ? 1 : 0);
    input.push_back(has_reader_mode ? 1 : 0);
    input.push_back(has_tab_grouping_suggestions ? 1 : 0);
    input.push_back(non_contextual_click_count);
    input.push_back(tab_group_shown_count);
    input.push_back(tab_group_clicked_count);
    return input;
  }

  ModelProvider::Response ExpectedResponse(
      bool has_price_insights,
      bool has_price_tracking,
      bool has_reader_mode,
      bool has_discounts = false,
      bool has_tab_grouping_suggestions = false,
      float non_contextual_click_count = 0,
      float tab_group_shown_count = 0,
      float tab_group_clicked_count = 0) {
    ModelProvider::Response response;
    response.push_back(has_discounts ? 1 : 0);
    response.push_back(has_price_insights ? 1 : 0);
    response.push_back(has_price_tracking ? 1 : 0);
    response.push_back(has_reader_mode ? 1 : 0);
    response.push_back(has_tab_grouping_suggestions ? 1 : 0);
    return response;
  }
};

TEST_F(ContextualPageActionsModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ContextualPageActionsModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();

  // Input vector empty.
  ModelProvider::Request input;
  ModelProvider::Response expected_response;
  ExpectExecutionWithInput(input, /*expected_error=*/true,
                           /*expected_result=*/expected_response);

  // Price insights = 0, price tracking = 0, reader mode = 0,
  input = GetRequestInput(false, false, false);
  expected_response = ExpectedResponse(false, false, false);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input, {});

  // Price insights = 1, price tracking = 0, reader mode = 0,
  input = GetRequestInput(true, false, false);
  expected_response = ExpectedResponse(true, false, false);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceInsights});

  // Price insights = 0, price tracking = 1, reader mode = 0,
  input = GetRequestInput(false, true, false);
  expected_response = ExpectedResponse(false, true, false);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceTracking});

  // Price insights = 0, price tracking = 0, reader mode = 1,
  input = GetRequestInput(false, false, true);
  expected_response = ExpectedResponse(false, false, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input, {kContextualPageActionModelLabelReaderMode});

  // Price insights = 1, price tracking = 0, reader mode = 1,
  input = GetRequestInput(true, false, true);
  expected_response = ExpectedResponse(true, false, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceInsights});

  // Price insights = 1, price tracking = 1, reader mode = 0,
  input = GetRequestInput(true, true, false);
  expected_response = ExpectedResponse(true, true, false);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceInsights});

  // Price insights = 0, price tracking = 1, reader mode = 1,
  input = GetRequestInput(false, true, true);
  expected_response = ExpectedResponse(false, true, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceTracking});

  // Price insights = 1, price tracking = 1, reader mode = 1,
  input = GetRequestInput(true, true, true);
  expected_response = ExpectedResponse(true, true, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceInsights});

  // Discounts = 1, Price insights = 1, price tracking = 1, reader mode = 1,
  input = GetRequestInput(true, true, true, true);
  expected_response = ExpectedResponse(true, true, true, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input, {kContextualPageActionModelLabelDiscounts});
}

TEST_F(ContextualPageActionsModelTest, ExecuteModelWithInput_TabSuggestions) {
  ExpectInitAndFetchModel();

  // Input vector empty.
  ModelProvider::Request input;
  ModelProvider::Response expected_response;

  // Price insights = 0, price tracking = 0, reader mode = 0, discounts = 0, tab
  // group suggestions = 1.
  input = GetRequestInput(false, false, false, false, true);
  expected_response = ExpectedResponse(false, false, false, false, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  ExpectClassifierResults(input, {kContextualPageActionModelLabelTabGrouping});

  // Price insights = 1, price tracking = 0, reader mode = 0, discounts = 1, tab
  // group suggestions = 1.
  input = GetRequestInput(true, false, false, true, true);
  expected_response = ExpectedResponse(true, false, false, true, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/expected_response);
  // Discounts has greater priority than tab group suggestions.
  ExpectClassifierResults(input, {kContextualPageActionModelLabelDiscounts});
}

TEST_F(ContextualPageActionsModelTest,
       TabSuggestionsThrottling_ThrottleOnNewTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kContextualPageActionTabGroupThrottling,
        {{"throttle_on_new_tab", "true"}}}},
      {});

  ExpectInitAndFetchModel();

  // Tab group suggestions = 1.
  ModelProvider::Request input =
      GetRequestInput(false, false, false, false, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {0, 0, 0, 0, 1});
  ExpectClassifierResults(input, {kContextualPageActionModelLabelTabGrouping});

  // Tab group suggestions = 1, non contextual click = 1. "group suggestions"
  // is ignored due to non contextual clicks.
  input = GetRequestInput(false, false, false, false, true, 1);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {0, 0, 0, 0, 0});
  ExpectClassifierResults(input, {});

  // Tab group shown but not clicked.
  input = GetRequestInput(false, false, false, false, true, 0, 1, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {0, 0, 0, 0, 1});
  ExpectClassifierResults(input, {kContextualPageActionModelLabelTabGrouping});
}

TEST_F(ContextualPageActionsModelTest,
       TabSuggestionsThrottling_ShowWhenNotClicked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kContextualPageActionTabGroupThrottling,
        {{"show_when_not_clicked_in_last_day", "true"}}}},
      {});

  ExpectInitAndFetchModel();

  // Tab group suggestions = 1.
  ModelProvider::Request input =
      GetRequestInput(false, false, false, false, true);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {0, 0, 0, 0, 1});
  ExpectClassifierResults(input, {kContextualPageActionModelLabelTabGrouping});

  // Price insights = 0, price tracking = 0, reader mode = 0, discounts = 0, tab
  // group suggestions = 1, non contextual click = 1. "group suggestions" is
  // not ignored (throttle_on_new_tab is not enabled).
  input = GetRequestInput(false, false, false, false, true, 1);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {0, 0, 0, 0, 1});
  ExpectClassifierResults(input, {kContextualPageActionModelLabelTabGrouping});

  // Tab group shown but not clicked.
  input = GetRequestInput(false, false, false, false, true, 0, 1, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {0, 0, 0, 0, 0});
  ExpectClassifierResults(input, {});
}

}  // namespace segmentation_platform
