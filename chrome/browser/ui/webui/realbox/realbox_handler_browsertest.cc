// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include <string>
#include <unordered_map>

#include <gtest/gtest.h>
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

class BrowserTestWithParam : public InProcessBrowserTest,
                             public testing::WithParamInterface<bool> {
 public:
  BrowserTestWithParam() = default;
  BrowserTestWithParam(const BrowserTestWithParam&) = delete;
  BrowserTestWithParam& operator=(const BrowserTestWithParam&) = delete;
  ~BrowserTestWithParam() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kNtpRealboxSuggestionAnswers);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(RealboxHandlerMatchIconTest,
                         BrowserTestWithParam,
                         testing::Bool());

// Tests that all Omnibox match vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, MatchVectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    const bool is_bookmark = BrowserTestWithParam::GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
    if (vector_icon.name == omnibox::kBlankIcon.name) {
      // An empty resource name is effectively a blank icon.
      EXPECT_TRUE(svg_name.empty());
    } else if (vector_icon.name == omnibox::kPedalIcon.name) {
      // Pedals are not supported in the NTP Realbox.
      EXPECT_TRUE(svg_name.empty());
    } else if (is_bookmark) {
      EXPECT_EQ("chrome://resources/images/icon_bookmark.svg", svg_name);
    } else {
      EXPECT_FALSE(svg_name.empty());
    }
  }
}

// Tests that all Omnibox Answer vector icons map to an equivalent SVG for use
// in the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, AnswerVectorIcons) {
  for (int answer_type = SuggestionAnswer::ANSWER_TYPE_DICTIONARY;
       answer_type != SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT;
       answer_type++) {
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(omnibox::kNtpRealboxSuggestionAnswers));
    AutocompleteMatch match;
    SuggestionAnswer answer;
    answer.set_type(answer_type);
    match.answer = answer;
    const bool is_bookmark = BrowserTestWithParam::GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
    if (is_bookmark) {
      EXPECT_EQ("chrome://resources/images/icon_bookmark.svg", svg_name);
    } else {
      EXPECT_FALSE(svg_name.empty());
      EXPECT_NE("search.svg", svg_name);
    }
  }
}

using RealboxHandlerPedalIconTest = InProcessBrowserTest;

// Tests that all Omnibox Pedal vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_F(RealboxHandlerPedalIconTest, PedalVectorIcons) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals =
      GetPedalImplementations(/*incognito=*/true, /*testing=*/true);
  for (auto const& it : pedals) {
    const scoped_refptr<OmniboxPedal> pedal = it.second;
    const gfx::VectorIcon& vector_icon = pedal->GetVectorIcon();
    const std::string& svg_name =
        RealboxHandler::PedalVectorIconToResourceName(vector_icon);
    EXPECT_FALSE(svg_name.empty());
  }
}
