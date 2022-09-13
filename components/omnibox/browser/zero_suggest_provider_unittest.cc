// Copyright 2014 The Chromium Authors
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
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

namespace {

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient()
      : template_url_service_(new TemplateURLService(nullptr, 0)),
        pref_service_(new TestingPrefServiceSimple()) {
    pref_service_->registry()->RegisterStringPref(
        omnibox::kZeroSuggestCachedResults, std::string());
    pref_service_->registry()->RegisterDictionaryPref(
        omnibox::kZeroSuggestCachedResultsWithURL, base::Value::Dict());
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
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  network::TestURLLoaderFactory* test_loader_factory() {
    return client_->test_url_loader_factory();
  }

  GURL GetSuggestURL(
      metrics::OmniboxEventProto::PageClassification page_classification,
      metrics::OmniboxFocusType focus_type,
      const std::string& page_url) {
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification = page_classification;
    search_terms_args.focus_type = focus_type;
    search_terms_args.current_page_url = page_url;
    return RemoteSuggestionsService::EndpointUrl(
        search_terms_args, client_->GetTemplateURLService());
  }

  AutocompleteInput OnFocusInputForNTP() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  AutocompleteInput PrefetchingInputForNTP() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_ZPS_PREFETCH,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  AutocompleteInput PrefixInputForNTP() {
    AutocompleteInput input(u"foobar", metrics::OmniboxEventProto::NTP_REALBOX,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    return input;
  }

  AutocompleteInput OnFocusInputForWeb() {
    std::string input_url = "https://example.com/";
    AutocompleteInput input(base::ASCIIToUTF16(input_url),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  AutocompleteInput OnClobberInputForWeb() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com/"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
    return input;
  }

  AutocompleteInput PrefetchingInputForWeb() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER_ZPS_PREFETCH,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com/"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
    return input;
  }

  AutocompleteInput PrefixInputForWeb() {
    AutocompleteInput input(u"foobar", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com/"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    return input;
  }

  AutocompleteInput OnFocusInputForSRP() {
    std::string input_url = "https://google.com/search?q=omnibox";
    AutocompleteInput input(base::ASCIIToUTF16(input_url),
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  AutocompleteInput OnClobberInputForSRP() {
    AutocompleteInput input(u"",
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://google.com/search?q=omnibox"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
    return input;
  }

  AutocompleteInput PrefetchingInputForSRP() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::SRP_ZPS_PREFETCH,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://google.com/search?q=omnibox"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
    return input;
  }

  AutocompleteInput PrefixInputForSRP() {
    AutocompleteInput input(u"foobar",
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://google.com/search?q=omnibox"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
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

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  template_url_service->Load();

  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service->GetDefaultSearchProvider()->GetEngineType(
                template_url_service->search_terms_data()));

  provider_ = ZeroSuggestProvider::Create(client_.get(), this);
  provider_did_notify_ = false;

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetching,
                            omnibox::kZeroSuggestPrefetchingOnSRP,
                            omnibox::kZeroSuggestPrefetchingOnWeb},
      /*disabled_features=*/{});
}

void ZeroSuggestProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  provider_did_notify_ = true;
}

TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsNTP) {
  AutocompleteInput onfocus_ntp_input = OnFocusInputForNTP();

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(false));

  // Enable on-focus zero-suggest for signed-out users.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(omnibox::kZeroSuggestOnNTPForSignedOutUsers);

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), onfocus_ntp_input));
    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteNoURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), onfocus_ntp_input));
  }
  // Disable on-focus zero-suggest for signed-out users.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(omnibox::kZeroSuggestOnNTPForSignedOutUsers);

    EXPECT_CALL(*client_, IsAuthenticated())
        .WillRepeatedly(testing::Return(false));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), onfocus_ntp_input));
    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteNoURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), onfocus_ntp_input));
  }
}

TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsContextualWebAndSRP) {
  AutocompleteInput prefix_web_input = PrefixInputForWeb();
  AutocompleteInput prefix_srp_input = PrefixInputForSRP();
  AutocompleteInput on_focus_web_input = OnFocusInputForWeb();
  AutocompleteInput on_focus_srp_input = OnFocusInputForSRP();
  AutocompleteInput on_clobber_web_input = OnClobberInputForWeb();
  AutocompleteInput on_clobber_srp_input = OnClobberInputForSRP();

  // Disable on-clobber for OTHER and SRP.
  // Enable on-focus for OTHER and SRP.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/
        {omnibox::kFocusTriggersContextualWebZeroSuggest,
         omnibox::kFocusTriggersSRPZeroSuggest},
        /*disabled_features=*/{
            omnibox::kClobberTriggersContextualWebZeroSuggest,
            omnibox::kClobberTriggersSRPZeroSuggest,
        });

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_web_input));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_srp_input));

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_web_input));
    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_srp_input));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_web_input));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_srp_input));
  }
  // Enable on-clobber and on-focus for OTHER.
  // Disable on-clobber and on-focus for SRP.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kClobberTriggersContextualWebZeroSuggest,
                              omnibox::kFocusTriggersContextualWebZeroSuggest},
        /*disabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest,
                               omnibox::kFocusTriggersSRPZeroSuggest});

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_web_input));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_srp_input));

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_web_input));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_srp_input));

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_web_input));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_srp_input));
  }
  // Enable on-clobber and on-focus for SRP.
  // Disable on-clobber and on-focus for OTHER.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest,
                              omnibox::kFocusTriggersSRPZeroSuggest},
        /*disabled_features=*/{
            omnibox::kClobberTriggersContextualWebZeroSuggest,
            omnibox::kFocusTriggersContextualWebZeroSuggest});

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_web_input));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_srp_input));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_web_input));
    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_srp_input));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_web_input));
    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_srp_input));
  }
}

TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsRequestEligibility) {
  base::HistogramTester histogram_tester;

  // Enable on-focus for SRP.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kFocusTriggersSRPZeroSuggest);

  EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), OnFocusInputForSRP()));
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                     0 /*kEligible*/, 1);

  // Invalid URLs cannot be sent in the zero-suggest request.
  AutocompleteInput on_focus_srp_input_ineligible_url = OnFocusInputForSRP();
  on_focus_srp_input_ineligible_url.set_current_url(GURL("chrome://history"));
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), on_focus_srp_input_ineligible_url));
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                     3 /*kGenerallyIneligible*/, 1);

  // Change the default search provider.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  TemplateURLData data;
  data.SetURL("https://www.example.com/?q={searchTerms}");
  data.suggestions_url = "https://www.example.com/suggest/?q={searchTerms}";
  auto* other_search_provider =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      other_search_provider);

  // Zero-suggest is not allowed for non-Google default search providers.
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), OnFocusInputForSRP()));
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                     2 /*kRemoteSendURLIneligible*/, 1);

  // Zero-suggest is not allowed for non-Google default search providers.
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), OnFocusInputForNTP()));
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                     1 /*kRequestNoUrlIneligible*/, 1);

  // Zero-suggest is not allowed for non-empty inputs.
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), PrefixInputForSRP()));
  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                     3 /*kGenerallyIneligible*/, 2);

  histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                    5);
}

TEST_F(ZeroSuggestProviderTest, ResultTypeToRunNTP) {
  AutocompleteInput onfocus_ntp_input = OnFocusInputForNTP();
  EXPECT_EQ(
      ZeroSuggestProvider::ResultType::kRemoteNoURL,
      ZeroSuggestProvider::ResultTypeToRun(client_.get(), onfocus_ntp_input));
}

TEST_F(ZeroSuggestProviderTest, ResultTypeToRunContextualWeb) {
  AutocompleteInput on_focus_input = OnFocusInputForWeb();
  AutocompleteInput on_clobber_input = OnClobberInputForWeb();

  // Disable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            omnibox::kFocusTriggersContextualWebZeroSuggest,
            omnibox::kClobberTriggersContextualWebZeroSuggest});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
  // Enable on-focus only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kFocusTriggersContextualWebZeroSuggest},
        /*disabled_features=*/{
            omnibox::kClobberTriggersContextualWebZeroSuggest});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
  // Enable on-clobber only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::
                                  kClobberTriggersContextualWebZeroSuggest},
        /*disabled_features=*/
        {omnibox::kFocusTriggersContextualWebZeroSuggest});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
  // Enable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/
        {omnibox::kFocusTriggersContextualWebZeroSuggest,
         omnibox::kClobberTriggersContextualWebZeroSuggest},
        /*disabled_features=*/{});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
}

TEST_F(ZeroSuggestProviderTest, ResultTypeToRunSRP) {
  AutocompleteInput on_focus_input = OnFocusInputForSRP();
  AutocompleteInput on_clobber_input = OnClobberInputForSRP();

  // Disable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{omnibox::kFocusTriggersSRPZeroSuggest,
                               omnibox::kClobberTriggersSRPZeroSuggest});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
  // Enable on-focus only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kFocusTriggersSRPZeroSuggest},
        /*disabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
  // Enable on-clobber only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest},
        /*disabled_features=*/
        {omnibox::kFocusTriggersSRPZeroSuggest});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kNone,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
  // Enable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/
        {omnibox::kFocusTriggersSRPZeroSuggest,
         omnibox::kClobberTriggersSRPZeroSuggest},
        /*disabled_features=*/{});

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_focus_input));

    EXPECT_EQ(
        ZeroSuggestProvider::ResultType::kRemoteSendURL,
        ZeroSuggestProvider::ResultTypeToRun(client_.get(), on_clobber_input));
  }
}

TEST_F(ZeroSuggestProviderTest, StartStopNTP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");

  // Make sure valid input starts the provider.
  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForNTP();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener since the
  // request was invalidated.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, StartStopSRP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kClobberTriggersSRPZeroSuggest);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = OnClobberInputForSRP();
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
      input.current_url().spec());

  // Make sure valid input starts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForSRP();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener since the
  // request was invalidated.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, StartStopWeb) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kClobberTriggersContextualWebZeroSuggest);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = OnClobberInputForWeb();
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                    metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                    input.current_url().spec());

  // Make sure valid input starts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForWeb();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener since the
  // request was invalidated.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRunNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 1 /*REQUEST_SENT*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      5 /*REMOTE_RESPONSE_CONVERTED_TO_MATCHES*/, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRunSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kClobberTriggersSRPZeroSuggest);

  AutocompleteInput input = OnClobberInputForSRP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
      input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      5 /*REMOTE_RESPONSE_CONVERTED_TO_MATCHES*/, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRunWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kClobberTriggersContextualWebZeroSuggest);

  AutocompleteInput input = OnClobberInputForWeb();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                    metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                    input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      5 /*REMOTE_RESPONSE_CONVERTED_TO_MATCHES*/, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestOmitAsynchronousMatchesTrueNTP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  AutocompleteInput input = OnFocusInputForNTP();
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider to not have notified the provider listener since the
  // request was not sent.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestOmitAsynchronousMatchesTrueSRP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kClobberTriggersSRPZeroSuggest);

  AutocompleteInput input = OnClobberInputForSRP();
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
      input.current_url().spec());

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider to not have notified the provider listener since the
  // request was not sent.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestOmitAsynchronousMatchesTrueWeb) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kClobberTriggersContextualWebZeroSuggest);

  AutocompleteInput input = OnClobberInputForWeb();
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                    metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                    input.current_url().spec());

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider to not have notified the provider listener since the
  // request was not sent.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResultsNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 1 /*REQUEST_SENT*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResultsSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kClobberTriggersSRPZeroSuggest);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = OnClobberInputForSRP();
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
      input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResultsWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kClobberTriggersContextualWebZeroSuggest);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = OnClobberInputForWeb();
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                    metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                    input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestReceivedEmptyResultsNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 1 /*REQUEST_SENT*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      5 /*REMOTE_RESPONSE_CONVERTED_TO_MATCHES*/, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored.
  EXPECT_EQ(empty_response,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestReceivedEmptyResultsSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kClobberTriggersSRPZeroSuggest);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = OnClobberInputForSRP();
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
      input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      5 /*REMOTE_RESPONSE_CONVERTED_TO_MATCHES*/, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored.
  EXPECT_EQ(empty_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestReceivedEmptyResultsWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kClobberTriggersContextualWebZeroSuggest);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = OnClobberInputForWeb();
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                    metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                    input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      4 /*REMOTE_RESPONSE_CACHED*/, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      5 /*REMOTE_RESPONSE_CONVERTED_TO_MATCHES*/, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored.
  EXPECT_EQ(empty_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestPrefetchThenNTPOnFocus) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  {
    base::HistogramTester histogram_tester;

    // Start a prefetch request.
    AutocompleteInput input = PrefetchingInputForNTP();
    provider_->StartPrefetch(input);
    EXPECT_TRUE(provider_->done());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::NTP_ZPS_PREFETCH,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        R"(["",["search4", "search5", "search6"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 3);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 1 /*REQUEST_SENT*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch",
        3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch",
        4 /*REMOTE_RESPONSE_CACHED*/, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response2,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
  }
  {
    base::HistogramTester histogram_tester;

    // Start a non-prefetch request.
    AutocompleteInput input = OnFocusInputForNTP();
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results from the cached response.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        R"(["",["search7", "search8", "search9"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response3);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
        0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 1 /*REQUEST_SENT*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
        3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
        4 /*REMOTE_RESPONSE_CACHED*/, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response3,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
  }
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestPrefetchThenSRPOnClobber) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kClobberTriggersSRPZeroSuggest);

  PrefService* prefs = client_->GetPrefs();

  {
    base::HistogramTester histogram_tester;

    // Start a prefetch request.
    AutocompleteInput input = PrefetchingInputForSRP();
    // Set up the pref to cache the response from the previous run.
    std::string json_response(
        R"(["",["search1", "search2", "search3"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        prefs, input.current_url().spec(), json_response);
    provider_->StartPrefetch(input);
    EXPECT_TRUE(provider_->done());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::SRP_ZPS_PREFETCH,
                      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        R"(["",["search4", "search5", "search6"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 3);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 1 /*REQUEST_SENT*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
        3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
        4 /*REMOTE_RESPONSE_CACHED*/, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response2,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
  {
    base::HistogramTester histogram_tester;

    // Start a non-prefetch request.
    AutocompleteInput input = OnClobberInputForSRP();
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results from the cached response.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::
                          SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        R"(["",["search7", "search8", "search9"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response3);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
        1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        4 /*REMOTE_RESPONSE_CACHED*/, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response3,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestPrefetchThenWebOnClobber) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable on-clobber ZPS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      omnibox::kClobberTriggersContextualWebZeroSuggest);

  PrefService* prefs = client_->GetPrefs();

  {
    base::HistogramTester histogram_tester;

    // Start a prefetch request.
    AutocompleteInput input = PrefetchingInputForWeb();
    // Set up the pref to cache the response from the previous run.
    std::string json_response(
        R"(["",["search1", "search2", "search3"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        prefs, input.current_url().spec(), json_response);
    provider_->StartPrefetch(input);
    EXPECT_TRUE(provider_->done());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::OTHER_ZPS_PREFETCH,
                      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        R"(["",["search4", "search5", "search6"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 3);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 1 /*REQUEST_SENT*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
        3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
        4 /*REMOTE_RESPONSE_CACHED*/, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response2,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
  {
    base::HistogramTester histogram_tester;

    // Start a non-prefetch request.
    AutocompleteInput input = OnClobberInputForWeb();
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results from the cached response.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                      metrics::OmniboxFocusType::INTERACTION_CLOBBER,
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        R"(["",["search7", "search8", "search9"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response3);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        0 /*CACHED_RESPONSE_CONVERTED_TO_MATCHES*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 1 /*REQUEST_SENT*/,
        1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        3 /*REMOTE_RESPONSE_RECEIVED*/, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        4 /*REMOTE_RESPONSE_CACHED*/, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response3,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
}
