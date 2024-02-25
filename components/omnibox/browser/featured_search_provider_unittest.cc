// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/featured_search_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

class FeaturedSearchProviderTest : public testing::Test {
 protected:
  struct TestData {
    const std::u16string input;
    const std::vector<GURL> output;
  };

  FeaturedSearchProviderTest() : provider_(nullptr) {}
  ~FeaturedSearchProviderTest() override {}
  FeaturedSearchProviderTest(const FeaturedSearchProviderTest&) = delete;
  FeaturedSearchProviderTest& operator=(const FeaturedSearchProviderTest&) =
      delete;

  void SetUp() override {
    client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_template_url_service(
        std::make_unique<TemplateURLService>(nullptr, 0));
    provider_ = new FeaturedSearchProvider(client_.get());
  }
  void TearDown() override { provider_ = nullptr; }

  void RunTest(const TestData cases[], size_t num_cases) {
    ACMatches matches;
    for (size_t i = 0; i < num_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
      AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                              TestSchemeClassifier());
      input.set_prevent_inline_autocomplete(true);
      provider_->Start(input, false);
      EXPECT_TRUE(provider_->done());
      matches = provider_->matches();
      ASSERT_EQ(cases[i].output.size(), matches.size());
      for (size_t j = 0; j < cases[i].output.size(); ++j) {
        EXPECT_EQ(cases[i].output[j], matches[j].destination_url);
      }
    }
  }

  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<FeaturedSearchProvider> provider_;
};

TEST_F(FeaturedSearchProviderTest, NonAtPrefix) {
  TestData test_cases[] = {
      // Typing text that doesn't start with "@" should give nothing.
      {u"g@rb@g3", {}},
      {u"www.google.com", {}},
      {u"http:www.google.com", {}},
      {u"http://www.google.com", {}},
      {u"file:filename", {}},
      {u"chrome:", {}},
      {u"chrome://", {}},
      {u"chrome://version", {}},
  };

  RunTest(test_cases, std::size(test_cases));
}

TEST_F(FeaturedSearchProviderTest, DoesNotSupportMatchesOnFocus) {
  AutocompleteInput input(u"@tabs", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(FeaturedSearchProviderTest, StarterPack) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kStarterPackExpansion);

  const GURL kBookmarksUrl =
      GURL(TemplateURLStarterPackData::bookmarks.destination_url);
  const GURL kHistoryUrl =
      GURL(TemplateURLStarterPackData::history.destination_url);
  const GURL kTabsUrl = GURL(TemplateURLStarterPackData::tabs.destination_url);

  const std::u16string kBookmarksKeyword = u"@bookmarks";
  const std::u16string kHistoryKeyword = u"@history";
  const std::u16string kTabsKeyword = u"@tabs";

  // Populate template URL with starter pack entries
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      TemplateURLStarterPackData::GetStarterPackEngines();
  for (auto& turl : turls) {
    client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }

  TestData typing_scheme_cases[] = {
      // Typing the keyword without '@' or past the keyword shouldn't produce
      // results.
      {u"b", {}},
      {u"bookmarks", {}},
      {u"his", {}},
      {u"history", {}},
      {u"@historyasdjflk", {}},
      {u"@bookmarksasld", {}},
      {u"tabs", {}},

      // With the expansion flag disabled, typing the `@gemini` keyword should
      // not provide the Gemini suggestion.
      {u"@gemini", {}},

      // Typing '@' should give all the starter pack suggestions.
      {u"@", {kBookmarksUrl, kHistoryUrl, kTabsUrl}},

      // Typing a portion of "@bookmarks" should give the bookmarks suggestion.
      {kBookmarksKeyword.substr(0, 3), {kBookmarksUrl}},
      {kBookmarksKeyword, {kBookmarksUrl}},

      // Typing a portion of "@history" should give the default urls.
      {kHistoryKeyword.substr(0, 3), {kHistoryUrl}},
      {kHistoryKeyword, {kHistoryUrl}},

      // Typing a portion of "@tabs" should give the default urls.
      {kTabsKeyword.substr(0, 3), {kTabsUrl}},
      {kTabsKeyword, {kTabsUrl}},
  };

  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest, StarterPackExpansion) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kStarterPackExpansion);

  const GURL kBookmarksUrl =
      GURL(TemplateURLStarterPackData::bookmarks.destination_url);
  const GURL kHistoryUrl =
      GURL(TemplateURLStarterPackData::history.destination_url);
  const GURL kTabsUrl = GURL(TemplateURLStarterPackData::tabs.destination_url);
  const GURL kAskGoogleUrl =
      GURL(TemplateURLStarterPackData::AskGoogle.destination_url);

  const std::u16string kBookmarksKeyword = u"@bookmarks";
  const std::u16string kHistoryKeyword = u"@history";
  const std::u16string kTabsKeyword = u"@tabs";
  const std::u16string kAskGoogleKeyword = u"@gemini";

  // Populate template URL with starter pack entries
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      TemplateURLStarterPackData::GetStarterPackEngines();
  for (auto& turl : turls) {
    client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }

  TestData typing_scheme_cases[] = {
      // Typing the keyword without '@' or past the keyword shouldn't produce
      // results.
      {u"b", {}},
      {u"bookmarks", {}},
      {u"his", {}},
      {u"history", {}},
      {u"@historyasdjflk", {}},
      {u"@bookmarksasld", {}},
      {u"tabs", {}},
      {u"gemi", {}},

      // Typing '@' should give all the starter pack suggestions.
      {u"@", {kBookmarksUrl, kAskGoogleUrl, kHistoryUrl, kTabsUrl}},

      // Typing a portion of "@bookmarks" should give the bookmarks suggestion.
      {kBookmarksKeyword.substr(0, 3), {kBookmarksUrl}},
      {kBookmarksKeyword, {kBookmarksUrl}},

      // Typing a portion of "@history" should give the default urls.
      {kHistoryKeyword.substr(0, 3), {kHistoryUrl}},
      {kHistoryKeyword, {kHistoryUrl}},

      // Typing a portion of "@tabs" should give the default urls.
      {kTabsKeyword.substr(0, 3), {kTabsUrl}},
      {kTabsKeyword, {kTabsUrl}},

      // Typing a portion of "@gemini" should give the default urls.
      {kAskGoogleKeyword.substr(0, 3), {kAskGoogleUrl}},
      {kAskGoogleKeyword, {kAskGoogleUrl}},
  };

  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest, StarterPackExpansionRelevance) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kStarterPackExpansion);

  const GURL kBookmarksUrl =
      GURL(TemplateURLStarterPackData::bookmarks.destination_url);
  const GURL kHistoryUrl =
      GURL(TemplateURLStarterPackData::history.destination_url);
  const GURL kTabsUrl = GURL(TemplateURLStarterPackData::tabs.destination_url);
  const GURL kAskGoogleUrl =
      GURL(TemplateURLStarterPackData::AskGoogle.destination_url);

  // Populate template URL with starter pack entries
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      TemplateURLStarterPackData::GetStarterPackEngines();
  for (auto& turl : turls) {
    client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }

  AutocompleteInput input(u"@", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  ACMatches matches = provider_->matches();
  ASSERT_EQ(turls.size(), matches.size());

  // Sort the matches according to relevances (in descending order), and make
  // sure that the matches are in the expected order.
  std::sort(matches.begin(), matches.end(), [](const auto& x, const auto& y) {
    return x.relevance > y.relevance;
  });

  GURL expected_match_order[] = {kAskGoogleUrl, kBookmarksUrl, kHistoryUrl,
                                 kTabsUrl};
  for (size_t i = 0; i < matches.size(); i++) {
    EXPECT_EQ(matches[i].destination_url, expected_match_order[i]);
  }
}
