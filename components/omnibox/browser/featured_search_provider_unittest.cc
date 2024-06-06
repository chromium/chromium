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
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

namespace {

constexpr char16_t kBookmarksKeyword[] = u"@bookmarks";
constexpr char16_t kHistoryKeyword[] = u"@history";
constexpr char16_t kTabsKeyword[] = u"@tabs";
constexpr char16_t kAskGoogleKeyword[] = u"@gemini";

constexpr char16_t kFeaturedKeyword1[] = u"@featured1";
constexpr char16_t kFeaturedKeyword2[] = u"@featured2";
constexpr char16_t kFeaturedKeyword3[] = u"@featured3";

const char* const kBookmarksUrl =
    TemplateURLStarterPackData::bookmarks.destination_url;
const char* const kHistoryUrl =
    TemplateURLStarterPackData::history.destination_url;
const char* const kTabsUrl = TemplateURLStarterPackData::tabs.destination_url;
const char* const kAskGoogleUrl =
    TemplateURLStarterPackData::AskGoogle.destination_url;

constexpr char kFeaturedUrl1[] = "https://featured1.com/q={searchTerms}";
constexpr char kFeaturedUrl2[] = "https://featured2.com/q={searchTerms}";
constexpr char kFeaturedUrl3[] = "https://featured3.com/q={searchTerms}";

struct TestData {
  const std::u16string input;
  const std::vector<std::string> output;
};

}  // namespace

class FeaturedSearchProviderTest : public testing::Test {
 protected:
  FeaturedSearchProviderTest() : provider_(nullptr) {}
  ~FeaturedSearchProviderTest() override {}
  FeaturedSearchProviderTest(const FeaturedSearchProviderTest&) = delete;
  FeaturedSearchProviderTest& operator=(const FeaturedSearchProviderTest&) =
      delete;

  void SetUp() override {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    client_->set_template_url_service(
        std::make_unique<TemplateURLService>(nullptr, 0));
    provider_ = new FeaturedSearchProvider(client_.get());
    omnibox::RegisterProfilePrefs(
        static_cast<TestingPrefServiceSimple*>(client_->GetPrefs())
            ->registry());
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
        EXPECT_EQ(GURL(cases[i].output[j]), matches[j].destination_url);
      }
    }
  }

  // Populate the TemplateURLService with starter pack entries.
  void AddStarterPackEntriesToTemplateUrlService() {
    std::vector<std::unique_ptr<TemplateURLData>> turls =
        TemplateURLStarterPackData::GetStarterPackEngines();
    for (auto& turl : turls) {
      client_->GetTemplateURLService()->Add(
          std::make_unique<TemplateURL>(std::move(*turl)));
    }
  }

  // Add a new featured search engine to the TemplateURLService.
  void AddFeaturedEnterpriseSearchEngine(const std::u16string& keyword,
                                         const std::string& url) {
    TemplateURLData template_url_data;
    template_url_data.SetKeyword(keyword);
    template_url_data.SetShortName(keyword + u" Name");
    template_url_data.SetURL(url);
    template_url_data.created_by_policy =
        TemplateURLData::CreatedByPolicy::kSiteSearch;
    template_url_data.enforced_by_policy = false;
    template_url_data.featured_by_policy = true;
    template_url_data.safe_for_autoreplace = false;

    client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(template_url_data));
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
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kStarterPackIPH);

  AutocompleteInput input(u"@tabs", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(FeaturedSearchProviderTest, StarterPack) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kStarterPackExpansion);

  AddStarterPackEntriesToTemplateUrlService();

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
      {std::u16string(kBookmarksKeyword, 0, 3), {kBookmarksUrl}},
      {kBookmarksKeyword, {kBookmarksUrl}},

      // Typing a portion of "@history" should give the default urls.
      {std::u16string(kHistoryKeyword, 0, 3), {kHistoryUrl}},
      {kHistoryKeyword, {kHistoryUrl}},

      // Typing a portion of "@tabs" should give the default urls.
      {std::u16string(kTabsKeyword, 0, 3), {kTabsUrl}},
      {kTabsKeyword, {kTabsUrl}},
  };

  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest, StarterPackExpansion) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kStarterPackExpansion);

  AddStarterPackEntriesToTemplateUrlService();
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
      {std::u16string(kBookmarksKeyword, 0, 3), {kBookmarksUrl}},
      {kBookmarksKeyword, {kBookmarksUrl}},

      // Typing a portion of "@history" should give the default urls.
      {std::u16string(kHistoryKeyword, 0, 3), {kHistoryUrl}},
      {kHistoryKeyword, {kHistoryUrl}},

      // Typing a portion of "@tabs" should give the default urls.
      {std::u16string(kTabsKeyword, 0, 3), {kTabsUrl}},
      {kTabsKeyword, {kTabsUrl}},

      // Typing a portion of "@gemini" should give the default urls.
      {std::u16string(kAskGoogleKeyword, 0, 3), {kAskGoogleUrl}},
      {kAskGoogleKeyword, {kAskGoogleUrl}},
  };

  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest, StarterPackExpansionRelevance) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kStarterPackExpansion);

  AddStarterPackEntriesToTemplateUrlService();

  AutocompleteInput input(u"@", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  ACMatches matches = provider_->matches();
  ASSERT_EQ(TemplateURLStarterPackData::GetStarterPackEngines().size(),
            matches.size());

  // Sort the matches according to relevances (in descending order), and make
  // sure that the matches are in the expected order.
  std::sort(matches.begin(), matches.end(), [](const auto& x, const auto& y) {
    return x.relevance > y.relevance;
  });

  std::string expected_match_order[] = {kAskGoogleUrl, kBookmarksUrl,
                                        kHistoryUrl, kTabsUrl};
  for (size_t i = 0; i < matches.size(); i++) {
    EXPECT_EQ(matches[i].destination_url, GURL(expected_match_order[i]));
  }
}

TEST_F(FeaturedSearchProviderTest, FeaturedEnterpriseSearch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({omnibox::kShowFeaturedEnterpriseSiteSearch,
                             omnibox::kStarterPackExpansion},
                            {});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword2, kFeaturedUrl2);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword1, kFeaturedUrl1);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword3, kFeaturedUrl3);

  TestData typing_scheme_cases[] = {
      // Typing the keyword without '@' or past the keyword shouldn't produce
      // results.
      {u"f", {}},
      {u"feat", {}},
      {u"featured1", {}},
      {u"featured2abs", {}},
      {u"@featured1xxa", {}},

      // Typing '@' should give all featured search engines and all the starter
      // pack suggestions. The provider does not change the order given
      // by TemplateURLService (which returns all keywords with the prefix in
      // alphabetical order). Re-ordering by relevance will be made
      // later on.
      {u"@",
       {kBookmarksUrl, kFeaturedUrl1, kFeaturedUrl2, kFeaturedUrl3,
        kAskGoogleUrl, kHistoryUrl, kTabsUrl}},

      // Typing a portion of "@featured" should give the featured engine
      // suggestions.
      {std::u16string(kFeaturedKeyword1, 0, 3),
       {kFeaturedUrl1, kFeaturedUrl2, kFeaturedUrl3}},
      {kFeaturedKeyword1, {kFeaturedUrl1}},
  };

  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest, ZeroSuggestStarterPackIPHSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kStarterPackIPH);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Starter Pack.
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(FeaturedSearchProvider::GetIPHType(matches[0]),
            FeaturedSearchProvider::IPHType::kGemini);

  // Not in ZPS, the IPH should not be provided.
  input.set_focus_type(metrics::INTERACTION_DEFAULT);
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);

  // "@" state - Confirm expected starter pack is still shown but no ZPS.
  AddStarterPackEntriesToTemplateUrlService();
  TestData typing_scheme_cases[] = {
      // Typing '@' should give all the starter pack suggestions, and no IPH.
      {u"@", {kBookmarksUrl, kAskGoogleUrl, kHistoryUrl, kTabsUrl}}};
  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestStarterPackIPHSuggestion_DeleteMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kStarterPackIPH);
  PrefService* prefs = client_->GetPrefs();

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Starter Pack.
  EXPECT_TRUE(prefs->GetBoolean(omnibox::kShowGeminiIPH));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(FeaturedSearchProvider::GetIPHType(matches[0]),
            FeaturedSearchProvider::IPHType::kGemini);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_FALSE(prefs->GetBoolean(omnibox::kShowGeminiIPH));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}

TEST_F(FeaturedSearchProviderTest, ZeroSuggestFeaturedSearchIPHSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({omnibox::kShowFeaturedEnterpriseSiteSearch,
                             omnibox::kShowFeaturedEnterpriseSiteSearchIPH,
                             omnibox::kStarterPackExpansion},
                            {omnibox::kStarterPackIPH});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword2, kFeaturedUrl2);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword1, kFeaturedUrl1);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword3, kFeaturedUrl3);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // featured Enterprise search.
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(FeaturedSearchProvider::GetIPHType(matches[0]),
            FeaturedSearchProvider::IPHType::kFeaturedEnterpriseSearch);

  // Not in ZPS, the IPH should not be provided.
  input.set_focus_type(metrics::INTERACTION_DEFAULT);
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);

  // "@" state - Confirm expected starter pack is still shown but no ZPS.
  TestData typing_scheme_cases[] = {
      // Typing '@' should give all the starter pack suggestions, and no IPH.
      {u"@",
       {kBookmarksUrl, kFeaturedUrl1, kFeaturedUrl2, kFeaturedUrl3,
        kAskGoogleUrl, kHistoryUrl, kTabsUrl}}};
  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestFeaturedSearchIPHSuggestion_DeleteMatch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({omnibox::kShowFeaturedEnterpriseSiteSearch,
                             omnibox::kShowFeaturedEnterpriseSiteSearchIPH,
                             omnibox::kStarterPackExpansion},
                            {omnibox::kStarterPackIPH});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword2, kFeaturedUrl2);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword1, kFeaturedUrl1);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword3, kFeaturedUrl3);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // featured Enterprise search.
  PrefService* prefs = client_->GetPrefs();
  EXPECT_TRUE(
      prefs->GetBoolean(omnibox::kShowFeaturedEnterpriseSiteSearchIPHPrefName));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(FeaturedSearchProvider::GetIPHType(matches[0]),
            FeaturedSearchProvider::IPHType::kFeaturedEnterpriseSearch);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_FALSE(
      prefs->GetBoolean(omnibox::kShowFeaturedEnterpriseSiteSearchIPHPrefName));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestStarerPackIPHAfterFeaturedSearchIPHDeleted) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {
          omnibox::kShowFeaturedEnterpriseSiteSearch,
          omnibox::kShowFeaturedEnterpriseSiteSearchIPH,
          omnibox::kStarterPackExpansion,
          omnibox::kStarterPackIPH,
      },
      {});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword2, kFeaturedUrl2);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword1, kFeaturedUrl1);
  AddFeaturedEnterpriseSearchEngine(kFeaturedKeyword3, kFeaturedUrl3);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // featured Enterprise search.
  PrefService* prefs = client_->GetPrefs();
  EXPECT_TRUE(
      prefs->GetBoolean(omnibox::kShowFeaturedEnterpriseSiteSearchIPHPrefName));
  EXPECT_TRUE(prefs->GetBoolean(omnibox::kShowGeminiIPH));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(FeaturedSearchProvider::GetIPHType(matches[0]),
            FeaturedSearchProvider::IPHType::kFeaturedEnterpriseSearch);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_FALSE(
      prefs->GetBoolean(omnibox::kShowFeaturedEnterpriseSiteSearchIPHPrefName));
  EXPECT_TRUE(prefs->GetBoolean(omnibox::kShowGeminiIPH));

  // Run the provider again, there should be one match corresponding to IPH for
  // Starter Pack.
  EXPECT_TRUE(prefs->GetBoolean(omnibox::kShowGeminiIPH));
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(FeaturedSearchProvider::GetIPHType(matches[0]),
            FeaturedSearchProvider::IPHType::kGemini);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_FALSE(
      prefs->GetBoolean(omnibox::kShowFeaturedEnterpriseSiteSearchIPHPrefName));
  EXPECT_FALSE(prefs->GetBoolean(omnibox::kShowGeminiIPH));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}
