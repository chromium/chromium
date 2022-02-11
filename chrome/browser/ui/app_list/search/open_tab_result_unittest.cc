// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/open_tab_result.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

}  //  namespace

class OpenTabResultTest : public testing::Test {
 public:
  OpenTabResultTest() {}

  OpenTabResultTest(const OpenTabResultTest&) = delete;
  OpenTabResultTest& operator=(const OpenTabResultTest&) = delete;

  ~OpenTabResultTest() override {}

  std::unique_ptr<OpenTabResult> MakeResult(const std::string& query,
                                            const std::string& contents,
                                            const std::string& url) {
    AutocompleteMatch match;
    match.contents = base::UTF8ToUTF16(contents);
    match.destination_url = GURL(url);
    TokenizedString tokenized_query(base::UTF8ToUTF16(query),
                                    TokenizedString::Mode::kWords);
    return std::make_unique<OpenTabResult>(nullptr, tokenized_query, match);
  }
};

TEST_F(OpenTabResultTest, Basic) {
  std::unique_ptr<OpenTabResult> result =
      MakeResult("query", "queryabc", "website.com");
  EXPECT_EQ(result->details(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT));
  EXPECT_GT(result->relevance(), 0.8);
}

}  // namespace app_list
