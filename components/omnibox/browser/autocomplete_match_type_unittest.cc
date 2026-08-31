// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"

namespace {

class FakeOmniboxPedal : public OmniboxPedal {
 public:
  FakeOmniboxPedal(OmniboxPedalId id, LabelStrings strings, GURL url)
      : OmniboxPedal(id, strings, url) {}

 private:
  ~FakeOmniboxPedal() override = default;
};

}  // namespace

TEST(AutocompleteMatchTypeTest, AccessibilityLabelHistory) {
  const std::u16string& kTestUrl = u"https://www.chromium.org";
  const std::u16string& kTestTitle = u"The Chromium Projects";

  // Test plain url.
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match.description = kTestTitle;
  EXPECT_EQ(kTestUrl + u", 2 of 9", AutocompleteMatchType::ToAccessibilityLabel(
                                        match, u"", kTestUrl, 1, 9));

  // Decorated with title and match type.
  match.type = AutocompleteMatchType::HISTORY_URL;
  EXPECT_EQ(
      kTestTitle + u" " + kTestUrl + u" location from history, 2 of 3",
      AutocompleteMatchType::ToAccessibilityLabel(match, u"", kTestUrl, 1, 3));
}

TEST(AutocompleteMatchTypeTest, AccessibilityLabelSearch) {
  const std::u16string& kSearch = u"gondola";
  const std::u16string& kTrendingSearchesHeader = u"Trending searches";
  const std::u16string& kSearchDesc = u"Google Search";

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.description = kSearchDesc;
  EXPECT_EQ(kSearch + u" search, " + kTrendingSearchesHeader + u", 6 of 8",
            AutocompleteMatchType::ToAccessibilityLabel(
                match, kTrendingSearchesHeader, kSearch, 5, 8));

  // Make sure there's no suffix if |total_matches| is 0, regardless of the
  // |match_index| value.
  EXPECT_EQ(kSearch + u" search, " + kTrendingSearchesHeader,
            AutocompleteMatchType::ToAccessibilityLabel(
                match, kTrendingSearchesHeader, kSearch, 5, 0));
}

TEST(AutocompleteMatchTypeTest, AccessibilityLabelPedal) {
  const std::u16string& kPedal = u"clear browsing data";
  const std::u16string& kAccessibilityHint =
      u"Clear your chrome browsing history, cookies, and cache";

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::PEDAL;
  const OmniboxAction::LabelStrings label_strings(
      /*hint=*/u"", /*suggestion_contents=*/u"", /*accessibility_suffix=*/u"",
      /*accessibility_hint=*/kAccessibilityHint);
  match.takeover_action = base::MakeRefCounted<FakeOmniboxPedal>(
      OmniboxPedalId::CLEAR_BROWSING_DATA, label_strings, GURL());

  // Ensure that the accessibility hint is present in the a11y label for pedal
  // suggestions.
  EXPECT_EQ(
      kAccessibilityHint + u", 2 of 5",
      AutocompleteMatchType::ToAccessibilityLabel(match, u"", kPedal, 1, 5));
}

namespace {

}  // namespace

TEST(AutocompleteMatchTypeTest, AccessibilityLabelThreadsHistory) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.subtypes.insert(
      omnibox::SuggestSubtype::SUBTYPE_AI_MODE_MORE_THREADS_ENTRYPOINT);

  std::u16string label_with_header =
      AutocompleteMatchType::ToAccessibilityLabel(
          match,
          /*header_text=*/u"menu item",
          /*match_text=*/u"View your AI Mode history",
          /*match_index=*/0,
          /*total_matches=*/1);

  EXPECT_EQ(label_with_header, u"View your AI Mode history, menu item, 1 of 1");
}
