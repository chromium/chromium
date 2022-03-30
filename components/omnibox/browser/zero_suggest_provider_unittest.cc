// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient()
      : template_url_service_(new TemplateURLService(nullptr, 0)),
        pref_service_(new TestingPrefServiceSimple()) {
    pref_service_->registry()->RegisterStringPref(
        omnibox::kZeroSuggestCachedResults, std::string());
  }
  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  bool SearchSuggestEnabled() const override { return true; }

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_.get();
  }

  TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }

  PrefService* GetPrefs() const override { return pref_service_.get(); }

  bool IsPersonalizedUrlDataCollectionActive() const override { return true; }

  void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) override {
    // Populate enough of |match| to keep the ZeroSuggestProvider happy.
    match->type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
    match->destination_url = GURL(text);
  }

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override {
    return scheme_classifier_;
  }

 private:
  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  TestSchemeClassifier scheme_classifier_;
};

}  // namespace

class ZeroSuggestProviderTest : public testing::TestWithParam<std::string>,
                                public AutocompleteProviderListener {
 public:
  ZeroSuggestProviderTest() = default;
  ZeroSuggestProviderTest(const ZeroSuggestProviderTest&) = delete;
  ZeroSuggestProviderTest& operator=(const ZeroSuggestProviderTest&) = delete;

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  network::TestURLLoaderFactory* test_loader_factory() {
    return client_->test_url_loader_factory();
  }

  GURL GetSuggestURL(
      metrics::OmniboxEventProto::PageClassification page_classification) {
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification = page_classification;
    search_terms_args.focus_type = OmniboxFocusType::ON_FOCUS;
    search_terms_args.zero_suggest_cache_duration_sec =
        OmniboxFieldTrial::kZeroSuggestCacheDurationSec.Get();
    return RemoteSuggestionsService::EndpointUrl(
        search_terms_args, client_->GetTemplateURLService());
  }

  AutocompleteInput CreateNTPOnFocusInputForRemoteNoUrl() {
    // Use NTP as the page classification, since REMOTE_NO_URL is enabled by
    // default for the NTP.
    AutocompleteInput input(
        std::u16string(),
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
        TestSchemeClassifier());
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    return input;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<ZeroSuggestProvider> provider_;
  bool provider_did_notify_;
};

void ZeroSuggestProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  TemplateURLService* turl_model = client_->GetTemplateURLService();
  turl_model->Load();

  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            turl_model->GetDefaultSearchProvider()->GetEngineType(
                turl_model->search_terms_data()));

  provider_ = ZeroSuggestProvider::Create(client_.get(), this);
  provider_did_notify_ = false;

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeatureWithParameters(
      omnibox::kZeroSuggestPrefetching,
      {{OmniboxFieldTrial::kZeroSuggestCacheDurationSec.name, GetParam()}});
}

void ZeroSuggestProviderTest::OnProviderUpdate(bool updated_matches) {
  provider_did_notify_ = true;
}

INSTANTIATE_TEST_SUITE_P(All,
                         ZeroSuggestProviderTest,
                         ::testing::ValuesIn({std::string("0"),
                                              std::string("60")}));

TEST_P(ZeroSuggestProviderTest, AllowZeroSuggestSuggestions) {
  std::string input_url = "https://example.com/";

  AutocompleteInput prefix_input(base::ASCIIToUTF16(input_url),
                                 metrics::OmniboxEventProto::OTHER,
                                 TestSchemeClassifier());
  prefix_input.set_focus_type(OmniboxFocusType::DEFAULT);

  AutocompleteInput on_focus_other(base::ASCIIToUTF16(input_url),
                                   metrics::OmniboxEventProto::OTHER,
                                   TestSchemeClassifier());
  on_focus_other.set_current_url(GURL(input_url));
  on_focus_other.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput on_focus_serp(
      base::ASCIIToUTF16(input_url),
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      TestSchemeClassifier());
  on_focus_serp.set_current_url(GURL(input_url));
  on_focus_serp.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput on_clobber_other(std::u16string(),
                                     metrics::OmniboxEventProto::OTHER,
                                     TestSchemeClassifier());
  on_clobber_other.set_current_url(GURL(input_url));
  on_clobber_other.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);

  AutocompleteInput on_clobber_serp(
      std::u16string(),
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      TestSchemeClassifier());
  on_clobber_serp.set_current_url(GURL(input_url));
  on_clobber_serp.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);

  // Disable on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            omnibox::kClobberTriggersContextualWebZeroSuggest,
            omnibox::kClobberTriggersSRPZeroSuggest,
        });

    // ZeroSuggest should never deal with prefix suggestions.
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(prefix_input));

    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_focus_other));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_focus_serp));

    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(on_clobber_other));
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(on_clobber_serp));
  }

  // Enable on-clobber for OTHER.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::
                                  kClobberTriggersContextualWebZeroSuggest},
        /*disabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest});
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(prefix_input));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_focus_other));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_clobber_other));
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(on_clobber_serp));
  }

  // Enable on-clobber for SRP.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest},
        /*disabled_features=*/{
            omnibox::kClobberTriggersContextualWebZeroSuggest});
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(prefix_input));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_focus_other));
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(on_clobber_other));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_clobber_serp));
  }
}

// TODO(tommycli): Break up this test into smaller ones.
TEST_P(ZeroSuggestProviderTest, TypeOfResultToRun) {
  // Verifies the unconfigured state. Returns platorm-specific defaults.
  // TODO(tommycli): The remote_no_url_allowed idiom seems kind of confusing,
  // its true meaning seems closer to "expect_remote_no_url". Ideally we can
  // simplify the test or make this much more obvious.
  auto ExpectPlatformSpecificDefaultZeroSuggestBehavior =
      [&](AutocompleteInput& input, const bool remote_no_url_allowed) {
        const auto current_page_classification =
            input.current_page_classification();
        GURL suggest_url = GetSuggestURL(current_page_classification);
        const auto result_type = ZeroSuggestProvider::TypeOfResultToRun(
            client_.get(), input, suggest_url);
        EXPECT_EQ(BaseSearchProvider::IsNTPPage(current_page_classification) &&
                          remote_no_url_allowed
                      ? ZeroSuggestProvider::ResultType::REMOTE_NO_URL
                      : ZeroSuggestProvider::ResultType::NONE,
                  result_type);
      };

  // Verify OTHER defaults (contextual web).
  std::string url("https://www.example.com/");
  AutocompleteInput other_input(base::ASCIIToUTF16(url),
                                metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
  other_input.set_current_url(GURL(url));
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
      /*remote_no_url_allowed=*/false);

  // Verify the platorm-specific defaults for the NTP.
  AutocompleteInput ntp_input(std::u16string(), metrics::OmniboxEventProto::NTP,
                              TestSchemeClassifier());
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      ntp_input,
#if BUILDFLAG(IS_IOS)
      /*remote_no_url_allowed=*/false);
#else
      /*remote_no_url_allowed=*/true);
#endif

  // Verify RemoteNoUrl works when the user is signed in.
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      ntp_input,
      /*remote_no_url_allowed=*/true);

  // But if the user has signed out, fall back to platform-specific defaults.
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(false));
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      /*remote_no_url_allowed=*/false);
#else
      /*remote_no_url_allowed=*/true);
#endif

  // Unless we allow remote suggestions for signed-out users.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kOmniboxTrendingZeroPrefixSuggestionsOnNTP);
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
      /*remote_no_url_allowed=*/true);

  // Restore authentication state, but now set a non-Google default search
  // engine. Verify that we still disallow remote suggestions.
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  TemplateURLData data;
  data.SetURL("https://www.example.com/?q={searchTerms}");
  data.suggestions_url = "https://www.example.com/suggest/?q={searchTerms}";
  auto* other_search_provider =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(other_search_provider);
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
      /*remote_no_url_allowed=*/false);
}

TEST_P(ZeroSuggestProviderTest, TypeOfResultToRunForContextualWeb) {
  std::string input_url = "https://example.com/";
  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER);

  AutocompleteInput on_focus_input(base::ASCIIToUTF16(input_url),
                                   metrics::OmniboxEventProto::OTHER,
                                   TestSchemeClassifier());
  on_focus_input.set_current_url(GURL(input_url));
  on_focus_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput on_clobber_input(std::u16string(),
                                     metrics::OmniboxEventProto::OTHER,
                                     TestSchemeClassifier());
  on_clobber_input.set_current_url(GURL(input_url));
  on_clobber_input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);

  const ZeroSuggestProvider::ResultType kDefaultContextualWebResultType =
#if BUILDFLAG(IS_ANDROID)
      ZeroSuggestProvider::ResultType::REMOTE_SEND_URL;
#else
      ZeroSuggestProvider::ResultType::NONE;
#endif

  const ZeroSuggestProvider::ResultType
      kDefaultContextualWebResultTypeOnClobber =
          ZeroSuggestProvider::ResultType::NONE;

  // Disable on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        omnibox::kClobberTriggersContextualWebZeroSuggest);

    EXPECT_EQ(kDefaultContextualWebResultType,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input, suggest_url));
    EXPECT_EQ(kDefaultContextualWebResultTypeOnClobber,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input, suggest_url));
  }

  // Enable on-focus only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        {omnibox::kOnFocusSuggestionsContextualWeb},         // Enabled
        {omnibox::kClobberTriggersContextualWebZeroSuggest}  // Disabled
    );

    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input, suggest_url));
    EXPECT_EQ(kDefaultContextualWebResultTypeOnClobber,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input, suggest_url));
  }
  // Enable on-clobber only.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        omnibox::kClobberTriggersContextualWebZeroSuggest);

    EXPECT_EQ(kDefaultContextualWebResultType,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input, suggest_url));
    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input, suggest_url));
  }
  // Enable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        {omnibox::kOnFocusSuggestionsContextualWeb,
         omnibox::kClobberTriggersContextualWebZeroSuggest},  // Enabled
        {}                                                    // Disabled
    );

    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input, suggest_url));
    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input, suggest_url));
  }
}

TEST_P(ZeroSuggestProviderTest, TestDoesNotReturnMatchesForPrefix) {
  // Use NTP because REMOTE_NO_URL is enabled by default for NTP.
  AutocompleteInput prefix_input(
      u"foobar input",
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  provider_->Start(prefix_input, false);

  // Expect that matches don't get populated out of cache because we are not
  // in zero suggest mode.
  EXPECT_TRUE(provider_->matches().empty());

  // Expect that loader did not get created.
  EXPECT_EQ(0, test_loader_factory()->NumPending());
}

TEST_P(ZeroSuggestProviderTest, TestStartWillStopForSomeInput) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  AutocompleteInput input = CreateNTPOnFocusInputForRemoteNoUrl();
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done_);

  // Make sure input stops the provider.
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done_);

  // Make sure invalid input stops the provider.
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done_);
  AutocompleteInput input2;
  provider_->Start(input2, false);
  EXPECT_TRUE(provider_->done_);
}

TEST_P(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRun) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  AutocompleteInput input = CreateNTPOnFocusInputForRemoteNoUrl();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_P(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResults) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = CreateNTPOnFocusInputForRemoteNoUrl();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

    // Expect that matches get populated synchronously out of the cache.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search1", provider_->matches()[0].contents);
    EXPECT_EQ(u"search2", provider_->matches()[1].contents);
    EXPECT_EQ(u"search3", provider_->matches()[2].contents);

    GURL suggest_url = GetSuggestURL(
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        "[\"\",[\"search4\", \"search5\", \"search6\"],"
        "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
        "\"google:verbatimrelevance\":1300}]");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect the provider to have notified the provider listener.
    EXPECT_TRUE(provider_did_notify_);

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestRequests.NonPrefetch",
                                      2);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestRequests.NonPrefetch",
        1 /*ZERO_SUGGEST_REQUEST_SENT*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestRequests.NonPrefetch",
        3 /*ZERO_SUGGEST_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestRequests.NonPrefetch.RoundTripTime", 1);
    histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestRequests.Prefetch",
                                      0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestRequests.Prefetch.RoundTripTime", 0);

    // Expect the same results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search1", provider_->matches()[0].contents);
    EXPECT_EQ(u"search2", provider_->matches()[1].contents);
    EXPECT_EQ(u"search3", provider_->matches()[2].contents);

    // Expect the new results to have been stored.
    EXPECT_EQ(json_response2,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_P(ZeroSuggestProviderTest, TestPsuggestZeroSuggestReceivedEmptyResults) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = CreateNTPOnFocusInputForRemoteNoUrl();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

    // Expect that matches get populated synchronously out of the cache.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search1", provider_->matches()[0].contents);
    EXPECT_EQ(u"search2", provider_->matches()[1].contents);
    EXPECT_EQ(u"search3", provider_->matches()[2].contents);

    GURL suggest_url = GetSuggestURL(
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string empty_response("[\"\",[],[],[],{}]");
    test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect the provider to have notified the provider listener.
    EXPECT_TRUE(provider_did_notify_);

    // Expect that the matches have been cleared.
    ASSERT_TRUE(provider_->matches().empty());

    // Expect the new results to have been stored.
    EXPECT_EQ(empty_response,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_P(ZeroSuggestProviderTest, TestPsuggestZeroSuggestPrefetch) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      "[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = CreateNTPOnFocusInputForRemoteNoUrl();
  provider_->StartPrefetch(input);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      "[\"\",[\"search4\", \"search5\", \"search6\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestRequests.Prefetch", 2);
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestRequests.Prefetch",
                                     1 /*ZERO_SUGGEST_REQUEST_SENT*/, 1);
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestRequests.Prefetch",
                                     3 /*ZERO_SUGGEST_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestRequests.Prefetch.RoundTripTime", 1);
  histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestRequests.NonPrefetch",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestRequests.NonPrefetch.RoundTripTime", 0);

  // Expect the provider not to have notified the provider listener.
  EXPECT_FALSE(provider_did_notify_);

    // Expect the same results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search1", provider_->matches()[0].contents);
    EXPECT_EQ(u"search2", provider_->matches()[1].contents);
    EXPECT_EQ(u"search3", provider_->matches()[2].contents);

    // Expect the new results to have been stored.
    EXPECT_EQ(json_response2,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
}
