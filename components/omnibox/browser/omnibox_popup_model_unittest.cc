// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_model.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_controller.h"
#include "components/omnibox/browser/test_omnibox_view.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

class TestOmniboxPopupView : public OmniboxPopupView {
 public:
  ~TestOmniboxPopupView() override {}
  bool IsOpen() const override { return false; }
  void InvalidateLine(size_t line) override {}
  void OnLineSelected(size_t line) override {}
  void UpdatePopupAppearance() override {}
  void ProvideButtonFocusHint(size_t line) override {}
  void OnMatchIconUpdated(size_t match_index) override {}
  void OnDragCanceled() override {}
};

class TestOmniboxEditModel : public OmniboxEditModel {
 public:
  TestOmniboxEditModel(OmniboxView* view,
                       OmniboxEditController* controller,
                       std::unique_ptr<OmniboxClient> client)
      : OmniboxEditModel(view, controller, std::move(client)) {}
  bool PopupIsOpen() const override { return true; }
};

}  // namespace

class OmniboxPopupModelTest : public ::testing::Test {
 public:
  OmniboxPopupModelTest()
      : view_(&controller_),
        model_(&view_, &controller_, std::make_unique<TestOmniboxClient>()),
        popup_model_(&popup_view_, &model_) {}

  OmniboxEditModel* model() { return &model_; }
  OmniboxPopupModel* popup_model() { return &popup_model_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestOmniboxEditController controller_;
  TestOmniboxView view_;
  TestOmniboxEditModel model_;
  TestOmniboxPopupView popup_view_;
  OmniboxPopupModel popup_model_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupModelTest);
};

// This verifies that the new treatment of the user's selected match in
// |SetSelectedLine()| with removed |AutocompleteResult::Selection::empty()|
// is correct in the face of various replacement versions of |empty()|.
TEST_F(OmniboxPopupModelTest, SetSelectedLine) {
  ACMatches matches;
  for (size_t i = 0; i < 2; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }
  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_FALSE(popup_model()->has_selected_match());
  popup_model()->SetSelectedLine(0, true, false);
  EXPECT_FALSE(popup_model()->has_selected_match());
  popup_model()->SetSelectedLine(0, false, false);
  EXPECT_TRUE(popup_model()->has_selected_match());
}

TEST_F(OmniboxPopupModelTest, SetSelectedLineWithNoDefaultMatches) {
  // Creates a set of matches with NO matches allowed to be default.
  ACMatches matches;
  for (size_t i = 0; i < 2; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    matches.push_back(match);
  }
  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);

  popup_model()->OnResultChanged();
  EXPECT_EQ(OmniboxPopupModel::kNoMatch, popup_model()->selected_line());
  EXPECT_FALSE(popup_model()->has_selected_match());

  popup_model()->SetSelectedLine(0, false, false);
  EXPECT_EQ(0U, popup_model()->selected_line());
  EXPECT_TRUE(popup_model()->has_selected_match());

  popup_model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(1U, popup_model()->selected_line());
  EXPECT_TRUE(popup_model()->has_selected_match());

  popup_model()->ResetToInitialState();
  EXPECT_EQ(OmniboxPopupModel::kNoMatch, popup_model()->selected_line());
  EXPECT_FALSE(popup_model()->has_selected_match());
}

TEST_F(OmniboxPopupModelTest, PopupPositionChanging) {
  ACMatches matches;
  for (size_t i = 0; i < 3; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }
  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0u, model()->popup_model()->selected_line());
  // Test moving and wrapping down.
  for (size_t n : {1, 2, 0}) {
    model()->OnUpOrDownKeyPressed(1);
    EXPECT_EQ(n, model()->popup_model()->selected_line());
  }
  // And down.
  for (size_t n : {2, 1, 0}) {
    model()->OnUpOrDownKeyPressed(-1);
    EXPECT_EQ(n, model()->popup_model()->selected_line());
  }
}

TEST_F(OmniboxPopupModelTest, ComputeMatchMaxWidths) {
  int contents_max_width, description_max_width;
  const int separator_width = 10;
  const int kMinimumContentsWidth = 300;
  int contents_width, description_width, available_width;

  // Both contents and description fit fine.
  contents_width = 100;
  description_width = 50;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Contents should be given as much space as it wants up to 300 pixels.
  contents_width = 100;
  description_width = 50;
  available_width = 100;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // Description should be hidden if it's at least 75 pixels wide but doesn't
  // get 75 pixels of space.
  contents_width = 300;
  description_width = 100;
  available_width = 384;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // If contents and description are on separate lines, each can take the full
  // available width.
  contents_width = 300;
  description_width = 100;
  available_width = 384;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Both contents and description will be limited.
  contents_width = 310;
  description_width = 150;
  available_width = 400;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(kMinimumContentsWidth, contents_max_width);
  EXPECT_EQ(available_width - kMinimumContentsWidth - separator_width,
            description_max_width);

  // Contents takes all available space.
  contents_width = 400;
  description_width = 0;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(available_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // Large contents will be truncated but small description won't if two line
  // suggestion.
  contents_width = 400;
  description_width = 100;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(available_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Large description will be truncated but small contents won't if two line
  // suggestion.
  contents_width = 100;
  description_width = 400;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(available_width, description_max_width);

  // Half and half.
  contents_width = 395;
  description_width = 395;
  available_width = 700;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(345, contents_max_width);
  EXPECT_EQ(345, description_max_width);

  // When we disallow shrinking the contents, it should get as much space as
  // it wants.
  contents_width = 395;
  description_width = 395;
  available_width = 700;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, false, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ((available_width - contents_width - separator_width),
            description_max_width);

  // (available_width - separator_width) is odd, so contents gets the extra
  // pixel.
  contents_width = 395;
  description_width = 395;
  available_width = 699;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(345, contents_max_width);
  EXPECT_EQ(344, description_max_width);

  // Not enough space to draw anything.
  contents_width = 1;
  description_width = 1;
  available_width = 0;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(0, contents_max_width);
  EXPECT_EQ(0, description_max_width);
}

// Makes sure focus remains on the tab switch button when nothing changes,
// and leaves when it does. Exercises the ratcheting logic in
// OmniboxPopupModel::OnResultChanged().
TEST_F(OmniboxPopupModelTest, TestFocusFixing) {
  ACMatches matches;
  AutocompleteMatch match(nullptr, 1000, false,
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  match.contents = base::ASCIIToUTF16("match1.com");
  match.destination_url = GURL("http://match1.com");
  match.allowed_to_be_default_match = true;
  match.has_tab_match = true;
  matches.push_back(match);

  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  popup_model()->SetSelectedLine(0, true, false);
  // The default state should be unfocused.
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Focus the selection.
  popup_model()->SetSelectedLine(0, false, false);
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  EXPECT_EQ(OmniboxPopupModel::BUTTON_FOCUSED,
            popup_model()->selected_line_state());

  // Adding a match at end won't change that we selected first suggestion, so
  // shouldn't change focused state.
  matches[0].relevance = 999;
  // Give it a different name so not deduped.
  matches[0].contents = base::ASCIIToUTF16("match2.com");
  matches[0].destination_url = GURL("http://match2.com");
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(OmniboxPopupModel::BUTTON_FOCUSED,
            popup_model()->selected_line_state());

  // Changing selection should change focused state.
  popup_model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Changing selection to same selection might change state.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  // Letting routine filter selecting same line should not change it.
  popup_model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(OmniboxPopupModel::BUTTON_FOCUSED,
            popup_model()->selected_line_state());
  // Forcing routine to handle selecting same line should change it.
  popup_model()->SetSelectedLine(1, false, true);
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Adding a match at end will reset selection to first, so should change
  // selected line, and thus focus.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  matches[0].relevance = 999;
  matches[0].contents = base::ASCIIToUTF16("match3.com");
  matches[0].destination_url = GURL("http://match3.com");
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0U, popup_model()->selected_line());
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Prepending a match won't change selection, but since URL is different,
  // should clear the focus state.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  matches[0].relevance = 1100;
  matches[0].contents = base::ASCIIToUTF16("match4.com");
  matches[0].destination_url = GURL("http://match4.com");
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0U, popup_model()->selected_line());
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Selecting |kNoMatch| should clear focus.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  popup_model()->SetSelectedLine(OmniboxPopupModel::kNoMatch, false, false);
  popup_model()->OnResultChanged();
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());
}
