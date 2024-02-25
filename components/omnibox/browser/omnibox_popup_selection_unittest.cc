// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_selection.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using LineState = OmniboxPopupSelection::LineState;
using Direction = OmniboxPopupSelection::Direction;
using Step = OmniboxPopupSelection::Step;

class OmniboxPopupSelectionTest : public testing::Test {
 protected:
  void SetUp() override {
    features_.InitAndEnableFeature(omnibox::kOmniboxKeywordModeRefresh);
  }

 private:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(OmniboxPopupSelectionTest, SelectionWithKeywordMode) {
  const std::u16string test_keyword = u"@bookmarks";
  TestOmniboxClient client;
  CHECK(client.GetTemplateURLService());
  client.GetTemplateURLService()->Load();
  client.GetTemplateURLService()->RepairStarterPackEngines();
  CHECK(client.GetTemplateURLService()->GetTemplateURLForKeyword(test_keyword));

  TestingPrefServiceSimple pref_service;
  AutocompleteResult result;
  result.AppendMatches({
      {nullptr, 1000, false, AutocompleteMatchType::SEARCH_SUGGEST},
      {nullptr, 900, false, AutocompleteMatchType::STARTER_PACK},
  });
  result.match_at(1u)->associated_keyword = std::make_unique<AutocompleteMatch>(
      nullptr, 1000, false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  result.match_at(1u)->associated_keyword->keyword = test_keyword;

  OmniboxPopupSelection next = OmniboxPopupSelection(0).GetNextSelection(
      result, &pref_service, client.GetTemplateURLService(),
      Direction::kForward, Step::kWholeLine);
  EXPECT_EQ(next.line, 1u);
  EXPECT_EQ(next.state, LineState::KEYWORD_MODE);

  next = OmniboxPopupSelection(0u).GetNextSelection(
      result, &pref_service, client.GetTemplateURLService(),
      Direction::kForward, Step::kStateOrLine);
  EXPECT_EQ(next.line, 1u);
  EXPECT_EQ(next.state, LineState::KEYWORD_MODE);

  next = OmniboxPopupSelection(1u, LineState::KEYWORD_MODE)
             .GetNextSelection(result, &pref_service,
                               client.GetTemplateURLService(),
                               Direction::kForward, Step::kWholeLine);
  EXPECT_EQ(next.line, 0u);
  EXPECT_EQ(next.state, LineState::NORMAL);

  next = OmniboxPopupSelection(1u, LineState::KEYWORD_MODE)
             .GetNextSelection(result, &pref_service,
                               client.GetTemplateURLService(),
                               Direction::kForward, Step::kStateOrLine);
  EXPECT_EQ(next.line, 0u);
  EXPECT_EQ(next.state, LineState::NORMAL);
}
