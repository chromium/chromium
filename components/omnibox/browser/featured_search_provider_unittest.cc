// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/featured_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

namespace {

constexpr char16_t kBookmarksKeyword[] = u"@bookmarks";
constexpr char16_t kHistoryKeyword[] = u"@history";
constexpr char16_t kTabsKeyword[] = u"@tabs";
constexpr char16_t kGeminiKeyword[] = u"@gemini";

const std::string kBookmarksUrl =
    template_url_starter_pack_data::bookmarks.destination_url;
const std::string kHistoryUrl =
    template_url_starter_pack_data::history.destination_url;
const std::string kTabsUrl =
    template_url_starter_pack_data::tabs.destination_url;
const std::string kGeminiUrl =
    template_url_starter_pack_data::gemini.destination_url;
const std::string kPageUrl =
    template_url_starter_pack_data::page.destination_url;
const std::string kAiModeUrl =
    template_url_starter_pack_data::ai_mode.destination_url;

struct TestData {
  const std::u16string input;
  const std::vector<std::string> output;
};

struct IphData {
  const IphType iph_type;
  const std::u16string iph_contents;
  const std::u16string iph_link_text;
  const GURL iph_link_url;
};

std::u16string FeaturedKeywordN(int n) {
  return base::UTF8ToUTF16("@featured" + base::NumberToString(n));
}

std::string FeaturedUrlN(int n) {
  return "https://featured" + base::NumberToString(n) + ".com/q={searchTerms}";
}

}  // namespace

class FeaturedSearchProviderTest : public testing::Test {
 public:
  FeaturedSearchProviderTest(const FeaturedSearchProviderTest&) = delete;
  FeaturedSearchProviderTest& operator=(const FeaturedSearchProviderTest&) =
      delete;

 protected:
  FeaturedSearchProviderTest() = default;
  ~FeaturedSearchProviderTest() override = default;

  void SetUp() override {
    toolbelt_scoped_config_.Get().enabled = true;
    client_ = std::make_unique<FakeAutocompleteProviderClient>();

    MockAimEligibilityService* mock_aim_eligibility_service =
        static_cast<MockAimEligibilityService*>(
            client_->GetAimEligibilityService());
    EXPECT_CALL(*mock_aim_eligibility_service, IsAimEligible())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_aim_eligibility_service, IsAimLocallyEligible())
        .WillRepeatedly(testing::Return(true));
    provider_ =
        new FeaturedSearchProvider(client_.get(), /*show_iph_matches=*/true);
    omnibox::RegisterProfilePrefs(
        static_cast<sync_preferences::TestingPrefServiceSyncable*>(
            client_->GetPrefs())
            ->registry());
  }
  void TearDown() override { provider_ = nullptr; }

  void RunTest(const std::vector<TestData> cases) {
    ACMatches matches;
    for (size_t i = 0; i < cases.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
      AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                              TestSchemeClassifier());
      input.set_allow_exact_keyword_match(false);
      provider_->Start(input, false);
      EXPECT_TRUE(provider_->done());

      std::vector<GURL> actual_urls;
      std::ranges::transform(
          provider_->matches(), std::back_inserter(actual_urls),
          [](const auto& match) { return match.destination_url; });
      std::vector<GURL> expected_urls;
      std::ranges::transform(cases[i].output, std::back_inserter(expected_urls),
                             [](const std::string& url) { return GURL(url); });
      EXPECT_THAT(actual_urls, testing::ElementsAreArray(expected_urls));
    }
  }

  void RunAndVerifyIph(const AutocompleteInput& input,
                       const std::vector<IphData> expected_iphs) {
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    ACMatches matches = provider_->matches();
    if (matches.size() == expected_iphs.size()) {
      for (size_t j = 0; j < expected_iphs.size(); ++j) {
        EXPECT_EQ(matches[j].iph_type, expected_iphs[j].iph_type);
        EXPECT_EQ(matches[j].contents, expected_iphs[j].iph_contents);
        EXPECT_EQ(matches[j].iph_link_text, expected_iphs[j].iph_link_text);
        EXPECT_EQ(matches[j].iph_link_url, expected_iphs[j].iph_link_url);
      }
    } else {
      EXPECT_EQ(matches.size(), expected_iphs.size());
    }
  }

  void RunAndVerifyIphTypes(const AutocompleteInput& input,
                            const std::vector<IphType> expected_iph_types) {
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    ACMatches matches = provider_->matches();
    if (matches.size() == expected_iph_types.size()) {
      for (size_t j = 0; j < expected_iph_types.size(); ++j)
        EXPECT_EQ(matches[j].iph_type, expected_iph_types[j]);
    } else {
      EXPECT_EQ(matches.size(), expected_iph_types.size());
    }
  }

  // Populate the TemplateURLService with starter pack entries.
  void AddStarterPackEntriesToTemplateUrlService() {
    std::vector<std::unique_ptr<TemplateURLData>> turls =
        template_url_starter_pack_data::GetStarterPackEngines();
    for (auto& turl : turls) {
      client_->GetTemplateURLService()->Add(
          std::make_unique<TemplateURL>(std::move(*turl)));
    }
  }

  // Add a new featured search engine to the TemplateURLService.
  void AddFeaturedEnterpriseSearchEngine(
      const std::u16string& keyword,
      const std::string& url,
      const TemplateURLData::PolicyOrigin& policy_origin,
      const TemplateURLData::ActiveStatus& is_active =
          TemplateURLData::ActiveStatus::kTrue) {
    TemplateURLData template_url_data;
    template_url_data.SetKeyword(keyword);
    template_url_data.SetShortName(keyword + u" Name");
    template_url_data.SetURL(url);
    template_url_data.policy_origin = policy_origin;
    template_url_data.enforced_by_policy = true;
    template_url_data.featured_by_policy = true;
    template_url_data.safe_for_autoreplace = false;
    template_url_data.is_active = is_active;

    client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(template_url_data));
  }

  base::test::TaskEnvironment task_environment_;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::Toolbelt>
      toolbelt_scoped_config_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<FeaturedSearchProvider> provider_;
};

TEST_F(FeaturedSearchProviderTest, NonAtPrefix) {
  std::vector<TestData> test_cases = {
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

  RunTest(test_cases);
}

TEST_F(FeaturedSearchProviderTest, DoesNotSupportMatchesOnFocus) {
  history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
      base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
        parameters.omnibox_scoped = false;
      }));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings, {}}},
      {omnibox::kStarterPackIPH});

  AutocompleteInput input(u"@tabs", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(FeaturedSearchProviderTest, StarterPack) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({omnibox::kAiModeStartPack},
                            {omnibox::kStarterPackExpansion});

  AddStarterPackEntriesToTemplateUrlService();

  std::vector<TestData> typing_scheme_cases = {
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
      {u"@", {kAiModeUrl, kBookmarksUrl, kHistoryUrl, kTabsUrl}},

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

  RunTest(typing_scheme_cases);
}

TEST_F(FeaturedSearchProviderTest, StarterPackExpansion) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kAiModeStartPack}, {});

  AddStarterPackEntriesToTemplateUrlService();
  std::vector<TestData> typing_scheme_cases = {
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
      {u"@", {kAiModeUrl, kBookmarksUrl, kGeminiUrl, kHistoryUrl, kTabsUrl}},

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
      {std::u16string(kGeminiKeyword, 0, 3), {kGeminiUrl}},
      {kGeminiKeyword, {kGeminiUrl}},
  };

  RunTest(typing_scheme_cases);
}

TEST_F(FeaturedSearchProviderTest, StarterPackExpansionRelevance) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kAiModeStartPack}, {});
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      scoped_config;
  scoped_config.Get().starter_pack_page = true;

  AddStarterPackEntriesToTemplateUrlService();

  AutocompleteInput input(u"@", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  ACMatches matches = provider_->matches();
  ASSERT_EQ(template_url_starter_pack_data::GetStarterPackEngines().size(),
            matches.size());

  // Sort the matches according to relevances (in descending order), and make
  // sure that the matches are in the expected order.
  std::sort(matches.begin(), matches.end(), [](const auto& x, const auto& y) {
    return x.relevance > y.relevance;
  });

  auto expected_match_order = std::vector<std::string>{
      kAiModeUrl, kGeminiUrl, kHistoryUrl, kBookmarksUrl, kPageUrl, kTabsUrl,
  };
  ASSERT_EQ(matches.size(), expected_match_order.size());
  for (size_t i = 0; i < matches.size(); i++) {
    EXPECT_EQ(matches[i].destination_url, GURL(expected_match_order[i])) << i;
  }
}

TEST_F(FeaturedSearchProviderTest, FeaturedEnterpriseSearch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kAiModeStartPack}, {});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(2), FeaturedUrlN(2),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  // Inactive featured enterprise keywords should not be shown.
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(3), FeaturedUrlN(3),
                                    TemplateURLData::PolicyOrigin::kSiteSearch,
                                    TemplateURLData::ActiveStatus::kFalse);
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(5), FeaturedUrlN(5),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(
      FeaturedKeywordN(4), FeaturedUrlN(4),
      TemplateURLData::PolicyOrigin::kSearchAggregator);
  // At most 4 featured enterprise keywords should be shown.
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(6), FeaturedUrlN(6),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(7), FeaturedUrlN(7),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);

  std::vector<TestData> typing_scheme_cases = {
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
       {kAiModeUrl, kBookmarksUrl, FeaturedUrlN(1), FeaturedUrlN(2),
        FeaturedUrlN(4), FeaturedUrlN(5), kGeminiUrl, kHistoryUrl, kTabsUrl}},

      // Typing a portion of "@featured" should give the featured engine
      // suggestions.
      {std::u16string(FeaturedKeywordN(1), 0, 3),
       {FeaturedUrlN(1), FeaturedUrlN(2), FeaturedUrlN(4), FeaturedUrlN(5)}},
      {FeaturedKeywordN(1), {FeaturedUrlN(1)}},
  };

  RunTest(typing_scheme_cases);
}

TEST_F(FeaturedSearchProviderTest, ZeroSuggestStarterPackIPHSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kStarterPackIPH,
       omnibox::kAiModeStartPack},
      {});

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Starter Pack.
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kGemini);

  // Not in ZPS, the IPH should not be provided.
  input.set_focus_type(metrics::INTERACTION_DEFAULT);
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);

  // "@" state - Confirm expected starter pack is still shown but no ZPS.
  AddStarterPackEntriesToTemplateUrlService();
  std::vector<TestData> typing_scheme_cases = {
      // Typing '@' should give all the starter pack suggestions, and no IPH.
      {u"@", {kAiModeUrl, kBookmarksUrl, kGeminiUrl, kHistoryUrl, kTabsUrl}}};
  RunTest(typing_scheme_cases);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestStarterPackIPHSuggestion_DeleteMatch) {
  history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
      base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
        parameters.omnibox_scoped = false;
      }));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings, {}},
       {omnibox::kStarterPackIPH, {}}},
      {});
  PrefService* prefs = client_->GetPrefs();

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Starter Pack.
  EXPECT_FALSE(prefs->GetBoolean(omnibox::kDismissedGeminiIph));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kGemini);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_TRUE(prefs->GetBoolean(omnibox::kDismissedGeminiIph));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestFeaturedEnterpriseSiteSearchIPHSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kAiModeStartPack},
      {omnibox::kStarterPackIPH});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(2), FeaturedUrlN(2),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  // Inactive featured enterprise keywords should not be shown or included in
  // IPH.
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(3), FeaturedUrlN(3),
                                    TemplateURLData::PolicyOrigin::kSiteSearch,
                                    TemplateURLData::ActiveStatus::kFalse);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Enterprise search aggregator.
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kFeaturedEnterpriseSiteSearch);
  EXPECT_EQ(matches[0].contents,
            u"Type @ to search across featured1.com, featured2.com");

  // Not in ZPS, the IPH should not be provided.
  input.set_focus_type(metrics::INTERACTION_DEFAULT);
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);

  // "@" state - Confirm expected starter pack is still shown but no ZPS.
  std::vector<TestData> typing_scheme_cases = {
      // Typing '@' should give all the starter pack suggestions, and no IPH.
      {u"@",
       {kAiModeUrl, kBookmarksUrl, FeaturedUrlN(1), FeaturedUrlN(2), kGeminiUrl,
        kHistoryUrl, kTabsUrl}}};
  RunTest(typing_scheme_cases);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestFeaturedSiteSearchIPHSuggestion_DeleteMatch) {
  history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
      base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
        parameters.omnibox_scoped = false;
      }));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings, {}},
       {omnibox::kStarterPackExpansion, {}}},
      {omnibox::kStarterPackIPH});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(2), FeaturedUrlN(2),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  // Inactive featured enterprise keywords should not be shown or included in
  // IPH.
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(3), FeaturedUrlN(3),
                                    TemplateURLData::PolicyOrigin::kSiteSearch,
                                    TemplateURLData::ActiveStatus::kFalse);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // featured Enterprise site search.
  PrefService* prefs = client_->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kFeaturedEnterpriseSiteSearch);
  EXPECT_EQ(matches[0].contents,
            u"Type @ to search across featured1.com, featured2.com");

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_TRUE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestStarterPackIPHAfterFeaturedSiteSearchIPHDeleted) {
  history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
      base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
        parameters.omnibox_scoped = false;
      }));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings, {}},
       {omnibox::kStarterPackExpansion, {}},
       {omnibox::kStarterPackIPH, {}}},
      {});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(2), FeaturedUrlN(2),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  // Inactive featured enterprise keywords should not be shown or included in
  // IPH.
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(3), FeaturedUrlN(3),
                                    TemplateURLData::PolicyOrigin::kSiteSearch,
                                    TemplateURLData::ActiveStatus::kFalse);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // featured Enterprise site search.
  PrefService* prefs = client_->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  EXPECT_FALSE(prefs->GetBoolean(omnibox::kDismissedGeminiIph));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kFeaturedEnterpriseSiteSearch);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_TRUE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  EXPECT_FALSE(prefs->GetBoolean(omnibox::kDismissedGeminiIph));

  // Run the provider again, there should be one match corresponding to IPH for
  // Starter Pack.
  EXPECT_FALSE(prefs->GetBoolean(omnibox::kDismissedGeminiIph));
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kGemini);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_TRUE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  EXPECT_TRUE(prefs->GetBoolean(omnibox::kDismissedGeminiIph));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestEnterpriseSearchAggregatorIPHSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kAiModeStartPack},
      {omnibox::kStarterPackIPH});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(
      FeaturedKeywordN(4), FeaturedUrlN(4),
      TemplateURLData::PolicyOrigin::kSearchAggregator);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Enterprise search aggregator.
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kEnterpriseSearchAggregator);

  // Not in ZPS, the IPH should not be provided.
  input.set_focus_type(metrics::INTERACTION_DEFAULT);
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);

  // "@" state - Confirm expected starter pack is still shown but no ZPS.
  std::vector<TestData> typing_scheme_cases = {
      // Typing '@' should give all the starter pack suggestions, and no IPH.
      {u"@",
       {kAiModeUrl, kBookmarksUrl, FeaturedUrlN(1), FeaturedUrlN(4), kGeminiUrl,
        kHistoryUrl, kTabsUrl}}};
  RunTest(typing_scheme_cases);
}

TEST_F(FeaturedSearchProviderTest,
       ZeroSuggestEnterpriseSearchAggregatorIPHSuggestion_DeleteMatch) {
  history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
      base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
        parameters.omnibox_scoped = false;
      }));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings, {}},
       {omnibox::kStarterPackExpansion, {}}},
      {omnibox::kStarterPackIPH});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(
      FeaturedKeywordN(4), FeaturedUrlN(4),
      TemplateURLData::PolicyOrigin::kSearchAggregator);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Enterprise search aggregator.
  PrefService* prefs = client_->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kEnterpriseSearchAggregator);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_TRUE(prefs->GetBoolean(
      omnibox::kDismissedEnterpriseSearchAggregatorIphPrefName));

  // Run the provider again, IPH match should not be provided.
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
}
TEST_F(
    FeaturedSearchProviderTest,
    ZeroSuggestFeaturedSiteSearchIPHAfterEnterpriseSearchAggregatorIPHDeleted) {
  history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
      base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
        parameters.omnibox_scoped = false;
      }));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{history_embeddings::kHistoryEmbeddings, {}},
       {omnibox::kStarterPackExpansion, {}},
       {omnibox::kStarterPackIPH, {}}},
      {});

  AddStarterPackEntriesToTemplateUrlService();

  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(
      FeaturedKeywordN(4), FeaturedUrlN(4),
      TemplateURLData::PolicyOrigin::kSearchAggregator);

  // "Focus" omnibox with zero input to put us in Zero suggest mode.
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // Run the provider, there should be one match corresponding to IPH for
  // Enterprise search aggregator.
  PrefService* prefs = client_->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedEnterpriseSearchAggregatorIphPrefName));
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  provider_->Start(input, false);
  ACMatches matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kEnterpriseSearchAggregator);

  // Call `DeleteMatch()`, match should be deleted from `matches_` and the pref
  // should be set to false.
  provider_->DeleteMatch(matches[0]);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 0u);
  EXPECT_TRUE(prefs->GetBoolean(
      omnibox::kDismissedEnterpriseSearchAggregatorIphPrefName));
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));

  // Run the provider again, there should be one match corresponding to IPH for
  // featured Enterprise site search. The match should not include the search
  // aggregator keyword.
  EXPECT_FALSE(prefs->GetBoolean(
      omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName));
  provider_->Start(input, false);
  matches = provider_->matches();
  EXPECT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NULL_RESULT_MESSAGE);
  EXPECT_EQ(matches[0].iph_type, IphType::kFeaturedEnterpriseSiteSearch);
  EXPECT_EQ(matches[0].contents, u"Type @ to search across featured1.com");
}

TEST_F(FeaturedSearchProviderTest, HistoryEmbedding_Iphs) {
  // Setup.
  AddStarterPackEntriesToTemplateUrlService();

  AutocompleteInput zero_input(u"", metrics::OmniboxEventProto::OTHER,
                               TestSchemeClassifier());
  zero_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  AutocompleteInput non_zero_input(u"x", metrics::OmniboxEventProto::OTHER,
                                   TestSchemeClassifier());
  AutocompleteInput scope_input(u"@history", metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());

  auto mock_setting = [&](bool setting_visible, bool setting_opted_in) {
    CHECK(!setting_opted_in || setting_visible);
    EXPECT_CALL(*client_, IsHistoryEmbeddingsSettingVisible())
        .WillRepeatedly(testing::Return(setting_visible));
    EXPECT_CALL(*client_, IsHistoryEmbeddingsEnabled())
        .WillRepeatedly(testing::Return(setting_opted_in));
  };

  // No IPH is shown when the feature is disabled.
  {
    history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
        base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
          parameters.omnibox_scoped = false;
        }));
    base::test::ScopedFeatureList disabled_features;
    disabled_features.InitAndEnableFeatureWithParameters(
        history_embeddings::kHistoryEmbeddings, {});
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(zero_input, {});
    }
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(non_zero_input, {});
    }
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(scope_input, {});
    }
  }

  // '@history' promo is shown when embeddings is not opted-in (even if the
  // feature is enabled).
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({{history_embeddings::kHistoryEmbeddings}}, {});
    mock_setting(false, false);
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(zero_input,
                      {{IphType::kHistoryScopePromo,
                        u"Type @history to search your browsing history"}});
    }
    // Not shown for non-zero input.
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(non_zero_input, {});
    }

    // '@history' AI promo is shown when embeddings is opted-in.
    mock_setting(true, true);
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(
          zero_input,
          {{IphType::kHistoryEmbeddingsScopePromo,
            u"Type @history to search your browsing history, powered by AI"}});
    }
    // Not shown for non-zero input.
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(non_zero_input, {});
    }

    // chrome://settings/ai/historySearch promo shown when not opted-in and in
    // @history scope.
    mock_setting(true, false);
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(
          scope_input,
          {{IphType::kHistoryEmbeddingsSettingsPromo,
            // Should end with whitespace since there's a link following it.
            u"For a more powerful way to search your browsing history, turn "
            u"on ",
            u"History search, powered by AI",
            GURL("chrome://settings/ai/historySearch")}});
    }
    // Not shown for unscoped inputs. Zero input will show the '@history' promo
    // tested above, so just test `non_zero_input` here.
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(non_zero_input, {});
    }
    // Not shown if the setting isn't available.
    mock_setting(false, false);
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(scope_input, {});
    }

    // Disclaimer shown when opted-in and in @history scope.
    mock_setting(true, true);
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(
          scope_input,
          {{IphType::kHistoryEmbeddingsDisclaimer,
            // Should end with whitespace since there's a link following it.
            u"Your searches, best matches, and their page contents are sent to "
            u"Google and may be seen by human reviewers to improve this "
            u"feature. "
            u"This is an experimental feature and won't always get it right. ",
            u"Learn more", GURL("chrome://settings/ai/historySearch")}});
    }
    // Not shown for unscoped inputs. Zero input will show the '@history' AI
    // promo tested above, so just test `non_zero_input` here.
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(non_zero_input, {});
    }
  }

  // Not shown if omnibox entry is disabled, even if embeddings is overall
  // enabled.
  {
    history_embeddings::ScopedFeatureParametersForTesting feature_parameters(
        base::BindOnce([](history_embeddings::FeatureParameters& parameters) {
          parameters.omnibox_scoped = false;
        }));
    base::test::ScopedFeatureList features_without_omnibox;
    features_without_omnibox.InitAndEnableFeatureWithParameters(
        history_embeddings::kHistoryEmbeddings, {});
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(zero_input, {});
    }
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(non_zero_input, {});
    }
    {
      SCOPED_TRACE("");
      RunAndVerifyIph(scope_input, {});
    }
  }
}

TEST_F(FeaturedSearchProviderTest, IphShownLimit) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {{omnibox::kStarterPackIPH}, {history_embeddings::kHistoryEmbeddings}},
      {});
  AddStarterPackEntriesToTemplateUrlService();
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  auto test = [&](const AutocompleteInput& input,
                  const std::vector<IphType> expected_iph_types) {
    RunAndVerifyIphTypes(input, expected_iph_types);
    // Notify the provider which matches were shown.
    AutocompleteResult result;
    result.AppendMatches(provider_->matches());
    provider_->RegisterDisplayedMatches(result);
  };

  // Show up to 3 IPHs per session.
  {
    SCOPED_TRACE("");
    test(input, {IphType::kGemini});
  }
  {
    SCOPED_TRACE("");
    test(input, {IphType::kGemini});
  }
  {
    SCOPED_TRACE("");
    test(input, {IphType::kGemini});
  }
  {
    SCOPED_TRACE("");
    test(input, {});
  }

  // Start a new session, should see an IPH 3 more times. But not the same IPH
  // as before, since it already consumed its limit.
  provider_ =
      new FeaturedSearchProvider(client_.get(), /*show_iph_matches=*/true);
  {
    SCOPED_TRACE("");
    test(input, {IphType::kHistoryScopePromo});
  }
  {
    SCOPED_TRACE("");
    test(input, {IphType::kHistoryScopePromo});
  }
  {
    SCOPED_TRACE("");
    test(input, {IphType::kHistoryScopePromo});
  }
  {
    SCOPED_TRACE("");
    test(input, {});
  }
}

TEST_F(FeaturedSearchProviderTest, OffTheRecord_HistoryEmbeddings) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({history_embeddings::kHistoryEmbeddings},
                            {omnibox::kStarterPackIPH});
  AddStarterPackEntriesToTemplateUrlService();
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // By default, the @history promo should be shown.
  RunAndVerifyIphTypes(input, {IphType::kHistoryScopePromo});

  // The @history scope doesn't work in Incognito or guest mode, though, so it
  // doesn't make sense to promote it in these windows.
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  RunAndVerifyIphTypes(input, {});
}

TEST_F(FeaturedSearchProviderTest, OffTheRecord_FeaturedEnterpriseSearch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {omnibox::kStarterPackExpansion, omnibox::kAiModeStartPack},
      {omnibox::kStarterPackIPH});
  AddStarterPackEntriesToTemplateUrlService();
  AddFeaturedEnterpriseSearchEngine(FeaturedKeywordN(1), FeaturedUrlN(1),
                                    TemplateURLData::PolicyOrigin::kSiteSearch);
  AddFeaturedEnterpriseSearchEngine(
      FeaturedKeywordN(2), FeaturedUrlN(2),
      TemplateURLData::PolicyOrigin::kSearchAggregator);
  AutocompleteInput input;
  input.set_focus_type(metrics::INTERACTION_FOCUS);

  // The enterprise search aggregator scope doesn't work in Incognito or guest
  // mode. However, the match and IPH for enterprise site search engine should
  // still show.
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  RunAndVerifyIph(input, {{IphType::kFeaturedEnterpriseSiteSearch,
                           u"Type @ to search across featured1.com"}});

  // "@" state.
  std::vector<TestData> typing_scheme_cases = {
      // Typing '@' should give all the starter pack suggestions (excluding
      // history), featured site search engine, and no IPH.
      {u"@",
       {kAiModeUrl, kBookmarksUrl, FeaturedUrlN(1), kGeminiUrl, kTabsUrl}}};
  RunTest(typing_scheme_cases);
}
