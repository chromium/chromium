// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/open_tab_result.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
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
                                            const std::string& description,
                                            const std::string& url) {
    AutocompleteMatch match;
    match.description = base::UTF8ToUTF16(description);
    match.destination_url = GURL(url);
    TokenizedString tokenized_query(base::UTF8ToUTF16(query),
                                    TokenizedString::Mode::kWords);
    return std::make_unique<OpenTabResult>(nullptr, nullptr, nullptr,
                                           tokenized_query, match);
  }
};

TEST_F(OpenTabResultTest, Basic) {
  std::unique_ptr<OpenTabResult> result =
      MakeResult("query", "queryabc", "http://www.website.com");
  EXPECT_EQ(result->title(), u"queryabc");
  EXPECT_EQ(
      StringFromTextVector(result->details_text_vector()),
      base::StrCat({u"http://www.website.com/ - ",
                    l10n_util::GetStringUTF16(IDS_APP_LIST_OPEN_TAB_HINT)}));
  EXPECT_EQ(
      result->accessible_name(),
      base::StrCat({u"queryabc, http://www.website.com/, ",
                    l10n_util::GetStringUTF16(IDS_APP_LIST_OPEN_TAB_HINT)}));
}

}  // namespace app_list
