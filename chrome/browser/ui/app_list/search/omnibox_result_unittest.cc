// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/search_engines/template_url.h"

namespace app_list {
namespace test {

namespace {

const char kFullQuery[] = "Hello World";
const char16_t kFullQuery16[] = u"Hello World";
const char kExampleDescription[] = "A website";
const char16_t kExampleDescription16[] = u"A website";
const char kExampleUrl[] = "http://example.com/hello";
const int kRelevance = 750;
const double kAppListRelevance = 0.5;

const char kExampleKeyword[] = "example.com";

}  // namespace

class OmniboxResultTest : public AppListTestBase {
 public:
  OmniboxResultTest() {}

  OmniboxResultTest(const OmniboxResultTest&) = delete;
  OmniboxResultTest& operator=(const OmniboxResultTest&) = delete;

  ~OmniboxResultTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_controller_delegate_ =
        std::make_unique<::test::TestAppListControllerDelegate>();
  }

  std::unique_ptr<OmniboxResult> CreateOmniboxResult(
      const std::string& original_query,
      int relevance,
      const std::string& destination_url,
      const std::string& contents,
      const std::string& description,
      AutocompleteMatchType::Type type,
      const std::string& keyword) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(original_query));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.relevance = relevance;
    match.destination_url = GURL(destination_url);
    match.stripped_destination_url = match.destination_url;
    match.contents = base::UTF8ToUTF16(contents);
    match.description = base::UTF8ToUTF16(description);
    match.type = type;
    match.keyword = base::UTF8ToUTF16(keyword);

    return std::make_unique<OmniboxResult>(
        profile_.get(), app_list_controller_delegate_.get(), nullptr, nullptr,
        input_, match, false);
  }

  const GURL& GetLastOpenedUrl() const {
    return app_list_controller_delegate_->last_opened_url();
  }

 private:
  std::unique_ptr<::test::TestAppListControllerDelegate>
      app_list_controller_delegate_;
  AutocompleteInput input_;
};

TEST_F(OmniboxResultTest, Basic) {
  std::unique_ptr<OmniboxResult> result = CreateOmniboxResult(
      kFullQuery, kRelevance, kExampleUrl, kFullQuery, kExampleDescription,
      AutocompleteMatchType::HISTORY_URL, kExampleKeyword);

  EXPECT_EQ(kExampleDescription16, result->title());
  EXPECT_EQ(kFullQuery16, result->details());
  EXPECT_EQ(kAppListRelevance, result->relevance());

  result->Open(0);
  EXPECT_EQ(kExampleUrl, GetLastOpenedUrl().spec());
}

}  // namespace test
}  // namespace app_list
