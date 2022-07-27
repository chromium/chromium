// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/open_tab_result.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace app_list {

using ::ash::string_matching::TokenizedString;

class OpenTabResultTest : public testing::Test {
 public:
  OpenTabResultTest() {}

  OpenTabResultTest(const OpenTabResultTest&) = delete;
  OpenTabResultTest& operator=(const OpenTabResultTest&) = delete;

  ~OpenTabResultTest() override {}

  std::unique_ptr<OpenTabResult> MakeResult(const std::u16string& query,
                                            const std::u16string& description,
                                            const std::u16string& url) {
    AutocompleteMatch match;
    match.description = description;
    match.destination_url = GURL(url);
    match.relevance = 1000;
    TokenizedString tokenized_query(query, TokenizedString::Mode::kCamelCase);
    return std::make_unique<OpenTabResult>(
        /*profile=*/nullptr, /*list_controller=*/nullptr,
        crosapi::CreateResult(match, /*controller=*/nullptr,
                              /*favicon_cache=*/nullptr,
                              /*bookmark_model=*/nullptr, AutocompleteInput()),
        tokenized_query);
  }
};

TEST_F(OpenTabResultTest, Basic) {
  std::unique_ptr<OpenTabResult> result =
      MakeResult(u"query", u"queryabc", u"http://www.website.com");

  EXPECT_EQ(result->title(), u"queryabc");
  EXPECT_EQ(StringFromTextVector(result->details_text_vector()),
            base::StrCat({u"http://www.website.com/",
                          l10n_util::GetStringFUTF16(IDS_APP_LIST_OPEN_TAB_HINT,
                                                     u" - ")}));
  EXPECT_EQ(result->accessible_name(),
            base::StrCat({u"queryabc, http://www.website.com/",
                          l10n_util::GetStringFUTF16(IDS_APP_LIST_OPEN_TAB_HINT,
                                                     u", ")}));
}

TEST_F(OpenTabResultTest, ManuallyCalculateRelevance) {
  std::unique_ptr<OpenTabResult> result1 =
      MakeResult(u"query", u"queryabc", u"http://www.website.com");
  std::unique_ptr<OpenTabResult> result2 =
      MakeResult(u"queryabc", u"queryabc", u"http://www.website.com");

  // The results were given the same |match.relevance|, but the closer query
  // should have higher score.
  EXPECT_GT(result2->relevance(), result1->relevance());
}

}  // namespace app_list
