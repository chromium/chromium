// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_selection.h"

#include <stddef.h>

#include <string>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using LineState = OmniboxPopupSelection::LineState;
using Direction = OmniboxPopupSelection::Direction;
using Step = OmniboxPopupSelection::Step;

class OmniboxPopupSelectionTest : public testing::Test {
 protected:
  void SetUp() override {}

 private:
  base::test::TaskEnvironment task_environment_;
};

// Desktop has special selection handling for starter pack keyword mode.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(OmniboxPopupSelectionTest, SelectionWithKeywordMode) {
  bool aim_button_visible = false;
  const std::u16string test_keyword = u"@bookmarks";
  TestOmniboxClient client;
  CHECK(client.GetTemplateURLService());
  client.GetTemplateURLService()->Load();
  client.GetTemplateURLService()->RepairStarterPackEngines();
  CHECK(client.GetTemplateURLService()->GetTemplateURLForKeyword(test_keyword));

  TestingPrefServiceSimple pref_service;
  AutocompleteInput input;
  AutocompleteResult result;
  result.AppendMatches({
      {nullptr, 1000, false, AutocompleteMatchType::SEARCH_SUGGEST},
      {nullptr, 900, false, AutocompleteMatchType::STARTER_PACK},
      {nullptr, 800, false, AutocompleteMatchType::HISTORY_EMBEDDINGS},
  });
  result.match_at(1u)->associated_keyword = test_keyword;

  OmniboxPopupSelection next = OmniboxPopupSelection(0u).GetNextSelection(
      input, result, client.GetTemplateURLService(), aim_button_visible,
      Direction::kForward, Step::kWholeLine);
  EXPECT_EQ(next.line, 1u);
  EXPECT_EQ(next.state, LineState::KEYWORD_MODE);

  next = OmniboxPopupSelection(0u).GetNextSelection(
      input, result, client.GetTemplateURLService(), aim_button_visible,
      Direction::kForward, Step::kStateOrLine);
  EXPECT_EQ(next.line, 1u);
  EXPECT_EQ(next.state, LineState::KEYWORD_MODE);

  next = OmniboxPopupSelection(1u, LineState::KEYWORD_MODE)
             .GetNextSelection(input, result, client.GetTemplateURLService(),
                               aim_button_visible, Direction::kForward,
                               Step::kWholeLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  next = OmniboxPopupSelection(1u, LineState::KEYWORD_MODE)
             .GetNextSelection(input, result, client.GetTemplateURLService(),
                               aim_button_visible, Direction::kForward,
                               Step::kStateOrLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  next = OmniboxPopupSelection(2u).GetNextSelection(
      input, result, client.GetTemplateURLService(), aim_button_visible,
      Direction::kForward, Step::kWholeLine);
  EXPECT_EQ(next.line, 0u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  next = OmniboxPopupSelection(2u).GetNextSelection(
      input, result, client.GetTemplateURLService(), aim_button_visible,
      Direction::kForward, Step::kStateOrLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::FOCUSED_BUTTON_THUMBS_UP);

  next = OmniboxPopupSelection(2u, LineState::FOCUSED_BUTTON_THUMBS_UP)
             .GetNextSelection(input, result, client.GetTemplateURLService(),
                               aim_button_visible, Direction::kForward,
                               Step::kStateOrLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::FOCUSED_BUTTON_THUMBS_DOWN);

  next = OmniboxPopupSelection(2u, LineState::FOCUSED_BUTTON_THUMBS_DOWN)
             .GetNextSelection(input, result, client.GetTemplateURLService(),
                               aim_button_visible, Direction::kForward,
                               Step::kStateOrLine);
  EXPECT_EQ(next.line, 0u);
  EXPECT_EQ(next.state, LineState::NORMAL);
}

TEST_F(OmniboxPopupSelectionTest, SelectionWithAIMButton) {
  bool aim_button_visible = true;

  AutocompleteInput input;
  AutocompleteResult result;
  result.AppendMatches({
      {nullptr, 1000, false, AutocompleteMatchType::SEARCH_SUGGEST},
      {nullptr, 900, false, AutocompleteMatchType::HISTORY_URL},
      {nullptr, 800, false, AutocompleteMatchType::HISTORY_TITLE},
  });

  // In the typed input (non-zero suggest) case, the first match in the list
  // will be selected by default.
  OmniboxPopupSelection initial{0u, LineState::NORMAL};

  {
    // Whole line stepping should skip the AIM button and just select the next
    // match.
    OmniboxPopupSelection next = initial.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kWholeLine);
    EXPECT_EQ(next.line, 1u);
    EXPECT_EQ(next.state, LineState::NORMAL);
  }

  {
    // "Line or state" stepping should focus the AIM button associated with the
    // first match.
    OmniboxPopupSelection next = initial.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kStateOrLine);
    EXPECT_EQ(next.line, 0u);
    EXPECT_EQ(next.state, LineState::FOCUSED_BUTTON_AIM);

    // Then move to the next regular match.
    next = next.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kStateOrLine);
    EXPECT_EQ(next.line, 1u);
    EXPECT_EQ(next.state, LineState::NORMAL);

    // And then the one after that.
    next = next.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kStateOrLine);
    EXPECT_EQ(next.line, 2u);
    EXPECT_EQ(next.state, LineState::NORMAL);
  }
}

TEST_F(OmniboxPopupSelectionTest, SelectionWithAIMButtonZeroInput) {
  bool aim_button_visible = true;

  AutocompleteInput input;
  // INTERACTION_FOCUS indicates that there is no user input.
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  AutocompleteResult result;
  result.AppendMatches({
      {nullptr, 1000, false, AutocompleteMatchType::SEARCH_SUGGEST},
      {nullptr, 900, false, AutocompleteMatchType::HISTORY_URL},
      {nullptr, 800, false, AutocompleteMatchType::HISTORY_TITLE},
  });

  // In the zero suggest case, there is no default match, which is represented
  // by a `line` value of `kNoMatch`.
  OmniboxPopupSelection initial{OmniboxPopupSelection::kNoMatch,
                                LineState::NORMAL};

  {
    // Whole line stepping should skip the AIM button and just select the first
    // match.
    OmniboxPopupSelection next = initial.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kWholeLine);
    EXPECT_EQ(next.line, 0u);
    EXPECT_EQ(next.state, LineState::NORMAL);
  }

  {
    // "Line or state" stepping should focus the AIM button, which is first in
    // the selection order when we're in zero suggest state.
    OmniboxPopupSelection next = initial.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kStateOrLine);
    EXPECT_EQ(next.line, OmniboxPopupSelection::kNoMatch);
    EXPECT_EQ(next.state, LineState::FOCUSED_BUTTON_AIM);

    // Then move to the first regular match.
    next = next.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kStateOrLine);
    EXPECT_EQ(next.line, 0u);
    EXPECT_EQ(next.state, LineState::NORMAL);

    // And then the one after that.
    next = next.GetNextSelection(
        input, result, /*template_url_service=*/nullptr, aim_button_visible,
        Direction::kForward, Step::kStateOrLine);
    EXPECT_EQ(next.line, 1u);
    EXPECT_EQ(next.state, LineState::NORMAL);
  }
}

TEST_F(OmniboxPopupSelectionTest, SelectionWithIphDisclaimer) {
  AutocompleteInput input;
  AutocompleteResult result;
  result.AppendMatches({
      {nullptr, 1000, false, AutocompleteMatchType::SEARCH_SUGGEST},
      {nullptr, 900, false, AutocompleteMatchType::NULL_RESULT_MESSAGE},
      {nullptr, 800, false, AutocompleteMatchType::NULL_RESULT_MESSAGE},
      {nullptr, 700, false, AutocompleteMatchType::NULL_RESULT_MESSAGE},
  });
  // Regular IPH tip (not a disclaimer).
  result.match_at(1u)->iph_type = IphType::kGemini;
  // IPH disclaimer.
  result.match_at(2u)->iph_type = IphType::kHistoryEmbeddingsDisclaimer;
  result.match_at(2u)->iph_link_url =
      GURL("chrome://settings/ai/historySearch");
  // IPH settings promo.
  result.match_at(3u)->iph_type = IphType::kHistoryEmbeddingsSettingsPromo;
  result.match_at(3u)->iph_link_url =
      GURL("chrome://settings/ai/historySearch");

  // With whole line stepping, focus should skip the regular IPH tip (match 1)
  // and land on the IPH disclaimer (match 2).
  OmniboxPopupSelection next =
      OmniboxPopupSelection(0u, LineState::NORMAL)
          .GetNextSelection(input, result,
                            /*template_url_service=*/nullptr,
                            /*aim_button_visible=*/false, Direction::kForward,
                            Step::kWholeLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  // Next step should land on the IPH settings promo (match 3).
  next = next.GetNextSelection(input, result, /*template_url_service=*/nullptr,
                               /*aim_button_visible=*/false,
                               Direction::kForward, Step::kWholeLine);
  EXPECT_EQ(next.line, 3u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  // Stepping backward from the settings promo row should return to the
  // disclaimer row (match 2).
  next = OmniboxPopupSelection(3u, LineState::NORMAL)
             .GetNextSelection(input, result, /*template_url_service=*/nullptr,
                               /*aim_button_visible=*/false,
                               Direction::kBackward, Step::kWholeLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  // Stepping backward from the disclaimer row should return to match 0.
  next = OmniboxPopupSelection(2u, LineState::NORMAL)
             .GetNextSelection(input, result, /*template_url_service=*/nullptr,
                               /*aim_button_visible=*/false,
                               Direction::kBackward, Step::kWholeLine);
  EXPECT_EQ(next.line, 0u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  // With state or line stepping from the IPH disclaimer row, tab should focus
  // the IPH link on the disclaimer row.
  next = OmniboxPopupSelection(2u, LineState::NORMAL)
             .GetNextSelection(input, result, /*template_url_service=*/nullptr,
                               /*aim_button_visible=*/false,
                               Direction::kForward, Step::kStateOrLine);
  EXPECT_EQ(next.line, 2u);
  EXPECT_EQ(next.state, LineState::FOCUSED_IPH_LINK);

  // With state or line stepping from the IPH settings promo row, tab should
  // focus the IPH link on the settings promo row.
  next = OmniboxPopupSelection(3u, LineState::NORMAL)
             .GetNextSelection(input, result, /*template_url_service=*/nullptr,
                               /*aim_button_visible=*/false,
                               Direction::kForward, Step::kStateOrLine);
  EXPECT_EQ(next.line, 3u);
  EXPECT_EQ(next.state, LineState::FOCUSED_IPH_LINK);
}

TEST_F(OmniboxPopupSelectionTest, IsControlPresentOnMatch) {
  AutocompleteResult result;
  result.AppendMatches({
      {nullptr, 1000, false, AutocompleteMatchType::SEARCH_SUGGEST},
      {nullptr, 900, false, AutocompleteMatchType::NULL_RESULT_MESSAGE},
      {nullptr, 800, false, AutocompleteMatchType::NULL_RESULT_MESSAGE},
      {nullptr, 700, false, AutocompleteMatchType::NULL_RESULT_MESSAGE},
  });
  // Regular IPH tip (not a disclaimer).
  result.match_at(1u)->iph_type = IphType::kGemini;
  // IPH disclaimer.
  result.match_at(2u)->iph_type = IphType::kHistoryEmbeddingsDisclaimer;
  result.match_at(2u)->iph_link_url = GURL("chrome://settings");
  // IPH settings promo.
  result.match_at(3u)->iph_type = IphType::kHistoryEmbeddingsSettingsPromo;
  result.match_at(3u)->iph_link_url = GURL("chrome://settings");

  // Normal state:
  // Match 0 (SEARCH_SUGGEST) should have control present.
  EXPECT_TRUE(OmniboxPopupSelection(0u, LineState::NORMAL)
                  .IsControlPresentOnMatch(result));
  // Match 1 (NULL_RESULT_MESSAGE, not disclaimer) should NOT have control
  // present.
  EXPECT_FALSE(OmniboxPopupSelection(1u, LineState::NORMAL)
                   .IsControlPresentOnMatch(result));
  // Match 2 (NULL_RESULT_MESSAGE, disclaimer) should have control present.
  EXPECT_TRUE(OmniboxPopupSelection(2u, LineState::NORMAL)
                  .IsControlPresentOnMatch(result));
  // Match 3 (NULL_RESULT_MESSAGE, settings promo) should have control present.
  EXPECT_TRUE(OmniboxPopupSelection(3u, LineState::NORMAL)
                  .IsControlPresentOnMatch(result));
}

#endif
