// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/local_history_zero_suggest_provider.h"

#include <memory>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index_test_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using OmniboxFieldTrial::GetLocalHistoryZeroSuggestAgeThreshold;

namespace {

// Used to populate the URLDatabase.
struct TestURLData {
  const TemplateURL* search_provider;
  std::string search_terms;
  std::string additional_query_params;
  int age_in_seconds;
  int visit_count = 1;
  std::string title = "";
  int typed_count = 1;
  bool hidden = false;
};

// Used to validate expected autocomplete matches.
struct TestMatchData {
  std::string content;
  int relevance;
  bool allowed_to_be_default_match = false;
};

}  // namespace

class LocalHistoryZeroSuggestProviderTest
    : public testing::Test,
      public AutocompleteProviderListener {
 public:
  LocalHistoryZeroSuggestProviderTest() = default;
  ~LocalHistoryZeroSuggestProviderTest() override = default;
  LocalHistoryZeroSuggestProviderTest(
      const LocalHistoryZeroSuggestProviderTest&) = delete;
  LocalHistoryZeroSuggestProviderTest& operator=(
      const LocalHistoryZeroSuggestProviderTest&) = delete;

 protected:
  // testing::Test
  void SetUp() override {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    provider_ = base::WrapRefCounted(
        LocalHistoryZeroSuggestProvider::Create(client_.get(), this));

#if defined(OS_IOS)  // Only needed for iOS.
    SetZeroSuggestVariant(
        metrics::OmniboxEventProto::NTP_REALBOX,
        LocalHistoryZeroSuggestProvider::kZeroSuggestLocalVariant);
#endif

    // Add the fallback default search provider to the TemplateURLService so
    // that it gets a valid unique identifier. Make the newly added provider the
    // user selected default search provider.
    TemplateURL* default_provider = client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(default_search_provider()->data()));
    client_->GetTemplateURLService()->SetUserSelectedDefaultSearchProvider(
        default_provider);

    // Verify that Google is the default search provider.
    ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
              default_search_provider()->GetEngineType(
                  client_->GetTemplateURLService()->search_terms_data()));
  }
  void TearDown() override {
    provider_ = nullptr;
    client_.reset();
    scoped_feature_list_.reset();
    task_environment_.RunUntilIdle();
  }

  // AutocompleteProviderListener
  void OnProviderUpdate(bool updated_matches) override;

  // Configures the ZeroSuggestVariant field trial param with the given value
  // for the given context.
  void SetZeroSuggestVariant(PageClassification page_classification,
                             std::string zero_suggest_variant_value);

  // Fills the URLDatabase with search URLs using the provided information.
  void LoadURLs(const std::vector<TestURLData>& url_data_list);

  // Waits for history::HistoryService's async operations.
  void WaitForHistoryService();

  // Creates an input using the provided information and queries the provider.
  void StartProviderAndWaitUntilDone(const std::string& text,
                                     OmniboxFocusType focus_type,
                                     PageClassification page_classification);

  // Verifies that provider matches are as expected.
  void ExpectMatches(const std::vector<TestMatchData>& match_data_list);

  const TemplateURL* default_search_provider() {
    return client_->GetTemplateURLService()->GetDefaultSearchProvider();
  }

  base::test::TaskEnvironment task_environment_;
  // Used to spin the message loop until |provider_| is done with its async ops.
  std::unique_ptr<base::RunLoop> provider_run_loop_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<LocalHistoryZeroSuggestProvider> provider_;
};

void LocalHistoryZeroSuggestProviderTest::SetZeroSuggestVariant(
    PageClassification page_classification,
    std::string zero_suggest_variant_value) {
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeatureWithParameters(
      omnibox::kOnFocusSuggestions,
      {{std::string(OmniboxFieldTrial::kZeroSuggestVariantRule) + ":" +
            base::NumberToString(static_cast<int>(page_classification)) + ":*",
        zero_suggest_variant_value}});
}

void LocalHistoryZeroSuggestProviderTest::LoadURLs(
    const std::vector<TestURLData>& url_data_list) {
  const Time now = Time::Now();
  for (const auto& entry : url_data_list) {
    TemplateURLRef::SearchTermsArgs search_terms_args(
        base::UTF8ToUTF16(entry.search_terms));
    search_terms_args.additional_query_params.append(
        entry.additional_query_params);
    const auto& search_terms_data =
        client_->GetTemplateURLService()->search_terms_data();
    std::string search_url =
        entry.search_provider->url_ref().ReplaceSearchTerms(search_terms_args,
                                                            search_terms_data);
    client_->GetHistoryService()->AddPageWithDetails(
        GURL(search_url), base::UTF8ToUTF16(entry.title), entry.visit_count,
        entry.typed_count, now - TimeDelta::FromSeconds(entry.age_in_seconds),
        entry.hidden, history::SOURCE_BROWSED);
    client_->GetHistoryService()->SetKeywordSearchTermsForURL(
        GURL(search_url), entry.search_provider->id(),
        base::UTF8ToUTF16(entry.search_terms));
    WaitForHistoryService();
  }
}

void LocalHistoryZeroSuggestProviderTest::WaitForHistoryService() {
  history::BlockUntilHistoryProcessesPendingRequests(
      client_->GetHistoryService());

  // MemoryURLIndex schedules tasks to rebuild its index on the history thread.
  // Block here to make sure they are complete.
  BlockUntilInMemoryURLIndexIsRefreshed(client_->GetInMemoryURLIndex());
}

void LocalHistoryZeroSuggestProviderTest::StartProviderAndWaitUntilDone(
    const std::string& text = "",
    OmniboxFocusType focus_type = OmniboxFocusType::ON_FOCUS,
    PageClassification page_classification =
        metrics::OmniboxEventProto::NTP_REALBOX) {
  AutocompleteInput input(base::ASCIIToUTF16(text), page_classification,
                          TestSchemeClassifier());
  input.set_focus_type(focus_type);
  provider_->Start(input, false);
  if (!provider_->done()) {
    provider_run_loop_ = std::make_unique<base::RunLoop>();
    // Quits in OnProviderUpdate when the provider is done.
    provider_run_loop_->Run();
  }
}

void LocalHistoryZeroSuggestProviderTest::OnProviderUpdate(
    bool updated_matches) {
  if (provider_->done() && provider_run_loop_)
    provider_run_loop_->Quit();
}

void LocalHistoryZeroSuggestProviderTest::ExpectMatches(
    const std::vector<TestMatchData>& match_data_list) {
  ASSERT_EQ(match_data_list.size(), provider_->matches().size());
  size_t index = 0;
  for (const auto& expected : match_data_list) {
    const auto& match = provider_->matches()[index];
    EXPECT_EQ(expected.relevance, match.relevance);
    EXPECT_EQ(base::UTF8ToUTF16(expected.content), match.contents);
    EXPECT_EQ(expected.allowed_to_be_default_match,
              match.allowed_to_be_default_match);
    index++;
  }
}

// Tests that suggestions are returned only if when input is empty and focused.
TEST_F(LocalHistoryZeroSuggestProviderTest, Input) {
  base::HistogramTester histogram_tester;

  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  StartProviderAndWaitUntilDone(/*text=*/"blah");
  ExpectMatches({});

  // Following histograms should not be logged if zero-prefix suggestions are
  // not allowed.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractedCount", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTime", 0);

  StartProviderAndWaitUntilDone(/*text=*/"", OmniboxFocusType::DEFAULT);
  ExpectMatches({});

  // Following histograms should not be logged if zero-prefix suggestions are
  // not allowed.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractedCount", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTime", 0);

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});

  // Following histograms should be logged when zero-prefix suggestions are
  // allowed and the keyword search terms database is queried.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractedCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTime", 1);
  // Deletion histograms should not be logged unless a suggestion is deleted.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SyncDeleteTime", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime", 0);
}

// Tests that suggestions are returned only if user is not in an off-the-record
// context.
TEST_F(LocalHistoryZeroSuggestProviderTest, Incognito) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  EXPECT_CALL(*client_.get(), IsOffTheRecord())
      .Times(2)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));

  StartProviderAndWaitUntilDone();
  ExpectMatches({});

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});
}

// Tests that suggestions are returned regardless of the authentication state
// when omnibox::kOmniboxLocalZeroSuggestForAuthenticatedUsers is enabled.
#if defined(OS_IOS)
// Tests that enable additional features fail on iOS.
#define MAYBE_ZeroSuggestForAuthenticatedUsers_Enabled \
  DISABLED_ZeroSuggestForAuthenticatedUsers_Enabled
#else
#define MAYBE_ZeroSuggestForAuthenticatedUsers_Enabled \
  ZeroSuggestForAuthenticatedUsers_Enabled
#endif
TEST_F(LocalHistoryZeroSuggestProviderTest,
       MAYBE_ZeroSuggestForAuthenticatedUsers_Enabled) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeature(
      omnibox::kOmniboxLocalZeroSuggestForAuthenticatedUsers);

  EXPECT_CALL(*client_.get(), IsAuthenticated()).Times(0);

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});
}

// Tests that suggestions are returned for signed-out users only when
// when omnibox::kOmniboxLocalZeroSuggestForAuthenticatedUsers is disabled.
#if defined(OS_IOS)
// Tests that enable additional features fail on iOS.
#define MAYBE_ZeroSuggestForAuthenticatedUsers_Disabled \
  DISABLED_ZeroSuggestForAuthenticatedUsers_Disabled
#else
#define MAYBE_ZeroSuggestForAuthenticatedUsers_Disabled \
  ZeroSuggestForAuthenticatedUsers_Disabled
#endif
TEST_F(LocalHistoryZeroSuggestProviderTest,
       MAYBE_ZeroSuggestForAuthenticatedUsers_Disabled) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndDisableFeature(
      omnibox::kOmniboxLocalZeroSuggestForAuthenticatedUsers);

  EXPECT_CALL(*client_.get(), IsAuthenticated())
      .Times(2)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));

  StartProviderAndWaitUntilDone();
  ExpectMatches({});

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});
}

// Tests that suggestions are returned only if ZeroSuggestVariant is configured
// to return local history suggestions in the NTP.
#if defined(OS_IOS)
// Flaky thread check failure: https://crbug.com/1071877
#define MAYBE_ZeroSuggestVariant DISABLED_ZeroSuggestVariant
#else
#define MAYBE_ZeroSuggestVariant ZeroSuggestVariant
#endif
TEST_F(LocalHistoryZeroSuggestProviderTest, MAYBE_ZeroSuggestVariant) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  // Verify that local history zero-prefix suggestions are enabled by default
  // on Desktop and Android NTP.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  StartProviderAndWaitUntilDone();
#if !defined(OS_IOS)  // Enabled by default on Desktop and Android NTP.
  ExpectMatches({{"hello world", 500}});
#else
  ExpectMatches({});
#endif

  // Verify that local history zero-prefix suggestions are enabled on the NTP
  // only.
  SetZeroSuggestVariant(
      metrics::OmniboxEventProto::OTHER,
      LocalHistoryZeroSuggestProvider::kZeroSuggestLocalVariant);
  StartProviderAndWaitUntilDone(
      /*text=*/"", OmniboxFocusType::ON_FOCUS,
      /*page_classification=*/metrics::OmniboxEventProto::OTHER);
  ExpectMatches({});

  SetZeroSuggestVariant(metrics::OmniboxEventProto::NTP_REALBOX,
                        /*zero_suggest_variant_value=*/"blah");
  StartProviderAndWaitUntilDone();
#if !defined(OS_IOS)  // Enabled by default on Desktop and Android NTP.
  ExpectMatches({{"hello world", 500}});
#else
  ExpectMatches({});
#endif

  // Verify that reactive zero-prefix suggestions enable local history
  // zero-prefix suggestions on the NTP.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeature(
      omnibox::kReactiveZeroSuggestionsOnNTPRealbox);
  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeature(
      omnibox::kReactiveZeroSuggestionsOnNTPOmnibox);
  StartProviderAndWaitUntilDone();
#if !defined(OS_IOS)  // Enabled by default on Desktop and Android NTP.
  ExpectMatches({{"hello world", 500}});
#else
  ExpectMatches({});
#endif

  StartProviderAndWaitUntilDone(
      /*text=*/"", OmniboxFocusType::ON_FOCUS,
      /*page_classification=*/
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
  ExpectMatches({{"hello world", 500}});

  // Make sure disabling omnibox::kNewSearchFeatures disables zero suggest.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitWithFeatures(
      {omnibox::kReactiveZeroSuggestionsOnNTPOmnibox},  // Enables the provider.
      {omnibox::kNewSearchFeatures});
  StartProviderAndWaitUntilDone(
      /*text=*/"", OmniboxFocusType::ON_FOCUS,
      /*page_classification=*/
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
#if defined(OS_ANDROID)  // Enabled by default on Android NTP.
  ExpectMatches({{"hello world", 500}});
#else
  ExpectMatches({});
#endif

  // Verify that configuring the ZeroSuggestVariant param with "local" for the
  // NTP, enables local history zero-prefix suggestions for that context.
  SetZeroSuggestVariant(
      metrics::OmniboxEventProto::NTP_REALBOX,
      LocalHistoryZeroSuggestProvider::kZeroSuggestLocalVariant);
  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});

  SetZeroSuggestVariant(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      LocalHistoryZeroSuggestProvider::kZeroSuggestLocalVariant);
  StartProviderAndWaitUntilDone(
      /*text=*/"", OmniboxFocusType::ON_FOCUS,
      /*page_classification=*/
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
  ExpectMatches({{"hello world", 500}});
}

// Tests that search terms are extracted from the default search provider's
// search history only and only when Google is the default search provider.
TEST_F(LocalHistoryZeroSuggestProviderTest, DefaultSearchProvider) {
  auto* template_url_service = client_->GetTemplateURLService();
  auto* other_search_provider = template_url_service->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("other")));
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
      {other_search_provider, "does not matter", "&foo=bar", 1},
  });

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}});

  template_url_service->SetUserSelectedDefaultSearchProvider(
      other_search_provider);
  // Verify that Google is not the default search provider.
  ASSERT_NE(SEARCH_ENGINE_GOOGLE,
            default_search_provider()->GetEngineType(
                template_url_service->search_terms_data()));
  StartProviderAndWaitUntilDone();
  ExpectMatches({});
}

// Tests that extracted search terms are normalized (their whitespaces are
// collapsed, are lowercased and deduplicated) without loss of unicode encoding.
TEST_F(LocalHistoryZeroSuggestProviderTest, Normalization) {
  LoadURLs({
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "HELLO   WORLD  ", "&foo=bar4", 1},
      {default_search_provider(), "سلام دنیا", "&bar=baz", 2},
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "hello world", "&foo=bar3", 3},
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "hello   world", "&foo=bar2", 4},
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "hello   world", "&foo=bar1", 5},
      {default_search_provider(), "hello world", "&foo=bar", 6},
  });

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"سلام دنیا", 500}, {"hello world", 499}});
}

// Tests that the suggestions are ranked correctly.
TEST_F(LocalHistoryZeroSuggestProviderTest, Ranking) {
  int original_query_age =
      history::kAutocompleteDuplicateVisitIntervalThreshold.InSeconds() + 3;
  LoadURLs({
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "more recent search", "&bar=baz2",
       /*age_in_seconds=*/0},
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "less recent search", "&foo=bar3",
       /*age_in_seconds=*/1},
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "less recent search", "&foo=bar2",
       /*age_in_seconds=*/original_query_age - 1},
      {default_search_provider(), "more recent search", "&bar=baz",
       /*age_in_seconds=*/original_query_age},
      {default_search_provider(), "less recent search", "&foo=bar",
       /*age_in_seconds=*/original_query_age},
  });

  // With frecency ranking disabled, more recent searches are ranked higher.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitWithFeatures(
      {omnibox::kReactiveZeroSuggestionsOnNTPRealbox},  // Enables the provider.
      {omnibox::kOmniboxLocalZeroSuggestFrecencyRanking});

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"more recent search", 500}, {"less recent search", 499}});

  // With frecency ranking enabled, more recent searches are ranked higher when
  // searches are just as frequent.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitWithFeatures(
      {omnibox::kReactiveZeroSuggestionsOnNTPRealbox,  // Enables the provider.
       omnibox::kOmniboxLocalZeroSuggestFrecencyRanking},
      {});

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"more recent search", 500}, {"less recent search", 499}});

  // With frecency ranking enabled, more frequent searches are ranked higher
  // when searches are nearly as old.
  LoadURLs({
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "less recent search", "&foo=bar4",
       /*age_in_seconds=*/2, /*visit_count=*/5},
  });

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"less recent search", 500}, {"more recent search", 499}});
}

// Tests that suggestions are created from fresh search histories only and that
// the freshness threshold can be adjusted.
TEST_F(LocalHistoryZeroSuggestProviderTest, Freshness) {
  // Verify the default age threshold.
  base::Time age_threshold = GetLocalHistoryZeroSuggestAgeThreshold();
  EXPECT_EQ(history::kLowQualityMatchAgeLimitInDays,
            base::TimeDelta(base::Time::Now() - age_threshold).InDays());

  // Override the age threshold to 7 days.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {{omnibox::kReactiveZeroSuggestionsOnNTPRealbox,  // Enables the provider.
        {}},
       {omnibox::kOmniboxLocalZeroSuggestAgeThreshold,
        {{OmniboxFieldTrial::kOmniboxLocalZeroSuggestAgeThresholdParam, "7"}}}},
      {});
  base::Time new_age_threshold = GetLocalHistoryZeroSuggestAgeThreshold();
  EXPECT_EQ(7, base::TimeDelta(base::Time::Now() - new_age_threshold).InDays());

  int fresh = (Time::Now() - new_age_threshold).InSeconds() - 60;
  int stale = (Time::Now() - new_age_threshold).InSeconds() + 60;
  LoadURLs({
      {default_search_provider(), "stale search", "&foo=bar", stale},
      {default_search_provider(), "fresh search", "&foo=bar", fresh},
  });

  // With the new age threshold, one of the two searches qualifies as a
  // suggestion. With the old threshold, neither would have.
  StartProviderAndWaitUntilDone();
  ExpectMatches({{"fresh search", 500}});
}

// Tests that the provider supports deletion of matches.
TEST_F(LocalHistoryZeroSuggestProviderTest, Deletion) {
  base::HistogramTester histogram_tester;

  auto* template_url_service = client_->GetTemplateURLService();
  auto* other_search_provider = template_url_service->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("other")));
  LoadURLs({
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "hello   world", "&foo=bar1", 1},
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "HELLO   WORLD  ", "&foo=bar2", 2},
      // Issued too closely to the original query; will be ignored:
      {default_search_provider(), "hello world", "foo=bar3", 3},
      {default_search_provider(), "hello world", "foo=bar4", 4},
      {other_search_provider, "hello world", "", 5},
      {default_search_provider(), "not to be deleted", "", 6},
  });

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"hello world", 500}, {"not to be deleted", 499}});

  // The keyword search terms database should be queried for the search terms
  // submitted to the default search provider only; which are 2 unique
  // normalized search terms in this case.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractedCount", 2, 1);

  provider_->DeleteMatch(provider_->matches()[0]);

  // Histogram tracking the synchronous deletion duration should get logged
  // synchrnously.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SyncDeleteTime", 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime", 0);

  // Make sure the deletion takes effect immediately in the provider before the
  // history service asynchronously performs the deletion or even before the
  // provider is started again.
  ExpectMatches({{"not to be deleted", 499}});

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"not to be deleted", 500}});

  // Wait until the history service performs the deletion.
  WaitForHistoryService();

  // Histogram tracking the async deletion duration should get logged once the
  // HistoryService async task returns to the initiating thread.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime", 1);

  StartProviderAndWaitUntilDone();
  ExpectMatches({{"not to be deleted", 500}});

  // Make sure all the search terms for the default search provider that would
  // produce the deleted match are deleted.
  history::URLDatabase* url_db =
      client_->GetHistoryService()->InMemoryDatabase();
  std::vector<history::NormalizedKeywordSearchTermVisit> visits =
      url_db->GetMostRecentNormalizedKeywordSearchTerms(
          default_search_provider()->id(),
          GetLocalHistoryZeroSuggestAgeThreshold());
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(base::ASCIIToUTF16("not to be deleted"), visits[0].normalized_term);

  // Make sure search terms from other search providers that would produce the
  // deleted match are not deleted.
  visits = url_db->GetMostRecentNormalizedKeywordSearchTerms(
      other_search_provider->id(), GetLocalHistoryZeroSuggestAgeThreshold());
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(base::ASCIIToUTF16("hello world"), visits[0].normalized_term);
}
