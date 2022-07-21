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
        omnibox::kZeroSuggestCachedResults, "");
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
      metrics::OmniboxEventProto::PageClassification page_classification) {
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification = page_classification;
    search_terms_args.focus_type = OmniboxFocusType::ON_FOCUS;
    return RemoteSuggestionsService::EndpointUrl(
        search_terms_args, client_->GetTemplateURLService());
  }

  AutocompleteInput OnFocusInputForNTP() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_REALBOX,
                            TestSchemeClassifier());
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    return input;
  }

  AutocompleteInput PrefetchingInputForNTP() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP_ZPS_PREFETCH,
                            TestSchemeClassifier());
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    return input;
  }

  AutocompleteInput PrefixInputForNTP() {
    AutocompleteInput input(u"foobar", metrics::OmniboxEventProto::NTP_REALBOX,
                            TestSchemeClassifier());
    input.set_focus_type(OmniboxFocusType::DEFAULT);
    return input;
  }

  AutocompleteInput OnFocusInputForWeb() {
    std::string input_url = "https://example.com/";
    AutocompleteInput input(base::ASCIIToUTF16(input_url),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    return input;
  }

  AutocompleteInput OnClobberInputForWeb() {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com/"));
    input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);
    return input;
  }

  AutocompleteInput PrefixInputForWeb() {
    std::string input_url = "https://example.com/";
    AutocompleteInput input(base::ASCIIToUTF16(input_url),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(OmniboxFocusType::DEFAULT);
    return input;
  }

  AutocompleteInput OnFocusInputForSRP() {
    std::string input_url = "https://example.com/";
    AutocompleteInput input(base::ASCIIToUTF16(input_url),
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    return input;
  }

  AutocompleteInput OnClobberInputForSRP() {
    AutocompleteInput input(u"",
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com/"));
    input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);
    return input;
  }

  AutocompleteInput PrefixInputForSRP() {
    std::string input_url = "https://example.com/";
    AutocompleteInput input(base::ASCIIToUTF16(input_url),
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(OmniboxFocusType::DEFAULT);
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

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeature(omnibox::kZeroSuggestPrefetching);
}

void ZeroSuggestProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  provider_did_notify_ = true;
}

TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestions_NTP) {
  AutocompleteInput onfocus_ntp_input = OnFocusInputForNTP();

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(false));

  // Enable on-focus zero-suggest for signed-out users.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(omnibox::kZeroSuggestOnNTPForSignedOutUsers);

    ZeroSuggestProvider::ResultType result_type = ZeroSuggestProvider::NONE;
    ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), onfocus_ntp_input, &result_type);
    EXPECT_EQ(ZeroSuggestProvider::REMOTE_NO_URL, result_type);
  }
  // Disable on-focus zero-suggest for signed-out users.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(omnibox::kZeroSuggestOnNTPForSignedOutUsers);

    EXPECT_CALL(*client_, IsAuthenticated())
        .WillRepeatedly(testing::Return(false));

    ZeroSuggestProvider::ResultType result_type = ZeroSuggestProvider::NONE;
    ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), onfocus_ntp_input, &result_type);
    EXPECT_EQ(ZeroSuggestProvider::NONE, result_type);
  }
}

TEST_F(ZeroSuggestProviderTest,
       AllowZeroPrefixSuggestions_ContextualWebAndSRP) {
  AutocompleteInput prefix_web_input = PrefixInputForWeb();
  AutocompleteInput prefix_srp_input = PrefixInputForSRP();
  AutocompleteInput on_focus_web_input = OnFocusInputForWeb();
  AutocompleteInput on_focus_srp_input = OnFocusInputForSRP();
  AutocompleteInput on_clobber_web_input = OnClobberInputForWeb();
  AutocompleteInput on_clobber_srp_input = OnClobberInputForSRP();
  ZeroSuggestProvider::ResultType result_type;

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
        client_.get(), prefix_web_input, &result_type));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_srp_input, &result_type));

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_web_input, &result_type));
    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_srp_input, &result_type));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_web_input, &result_type));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_srp_input, &result_type));
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
        client_.get(), prefix_web_input, &result_type));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_srp_input, &result_type));

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_web_input, &result_type));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_srp_input, &result_type));

    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_web_input, &result_type));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_srp_input, &result_type));
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
        client_.get(), prefix_web_input, &result_type));
    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), prefix_srp_input, &result_type));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_web_input, &result_type));
    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_focus_srp_input, &result_type));

    EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_web_input, &result_type));
    EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
        client_.get(), on_clobber_srp_input, &result_type));
  }
}

TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestions_RequestEligibility) {
  base::HistogramTester histogram_tester;

  // Enable on-focus for SRP.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kFocusTriggersSRPZeroSuggest);

  AutocompleteInput on_focus_srp_input = OnFocusInputForSRP();
  ZeroSuggestProvider::ResultType result_type = ZeroSuggestProvider::NONE;
  EXPECT_TRUE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), on_focus_srp_input, &result_type));

  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggest.Eligible.OnFocusV2",
                                     0 /*ELIGIBLE*/, 1);

  AutocompleteInput on_focus_srp_input_ineligible_url = OnFocusInputForSRP();
  on_focus_srp_input_ineligible_url.set_current_url(GURL("chrome://history"));
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), on_focus_srp_input_ineligible_url, &result_type));

  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggest.Eligible.OnFocusV2",
                                     1 /*URL_INELIGIBLE*/, 1);

  // zero-suggest is not allowed for non-Google default search providers.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  TemplateURLData data;
  data.SetURL("https://www.example.com/?q={searchTerms}");
  data.suggestions_url = "https://www.example.com/suggest/?q={searchTerms}";
  auto* other_search_provider =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      other_search_provider);
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), on_focus_srp_input, &result_type));

  histogram_tester.ExpectBucketCount("Omnibox.ZeroSuggest.Eligible.OnFocusV2",
                                     2 /*GENERALLY_INELIGIBLE*/, 1);

  // zero-suggest is not allowed for non-empty inputs.
  AutocompleteInput prefix_srp_input = PrefixInputForSRP();
  EXPECT_FALSE(ZeroSuggestProvider::AllowZeroPrefixSuggestions(
      client_.get(), prefix_srp_input, &result_type));

  // The last case is not taken into account for eligibility metrics.
  histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggest.Eligible.OnFocusV2",
                                    3);
}

TEST_F(ZeroSuggestProviderTest, TypeOfResultToRun_NTP) {
  AutocompleteInput onfocus_ntp_input = OnFocusInputForNTP();
  EXPECT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            ZeroSuggestProvider::TypeOfResultToRun(
                client_.get(), onfocus_ntp_input,
                /*bypass_request_eligibility_checks=*/true));
}

TEST_F(ZeroSuggestProviderTest, TypeOfResultToRun_ContextualWeb) {
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

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
  // Enable on-focus only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kFocusTriggersContextualWebZeroSuggest},
        /*disabled_features=*/{
            omnibox::kClobberTriggersContextualWebZeroSuggest});

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
  // Enable on-clobber only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::
                                  kClobberTriggersContextualWebZeroSuggest},
        /*disabled_features=*/
        {omnibox::kFocusTriggersContextualWebZeroSuggest});

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
  // Enable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/
        {omnibox::kFocusTriggersContextualWebZeroSuggest,
         omnibox::kClobberTriggersContextualWebZeroSuggest},
        /*disabled_features=*/{});

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
}

TEST_F(ZeroSuggestProviderTest, TypeOfResultToRun_SRP) {
  AutocompleteInput on_focus_input = OnFocusInputForSRP();
  AutocompleteInput on_clobber_input = OnClobberInputForSRP();

  // Disable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{omnibox::kFocusTriggersSRPZeroSuggest,
                               omnibox::kClobberTriggersSRPZeroSuggest});

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
  // Enable on-focus only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kFocusTriggersSRPZeroSuggest},
        /*disabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest});

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
  // Enable on-clobber only.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kClobberTriggersSRPZeroSuggest},
        /*disabled_features=*/
        {omnibox::kFocusTriggersSRPZeroSuggest});

    EXPECT_EQ(ZeroSuggestProvider::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
  // Enable on-focus and on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/
        {omnibox::kFocusTriggersSRPZeroSuggest,
         omnibox::kClobberTriggersSRPZeroSuggest},
        /*disabled_features=*/{});

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input,
                  /*bypass_request_eligibility_checks=*/true));

    EXPECT_EQ(ZeroSuggestProvider::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input,
                  /*bypass_request_eligibility_checks=*/true));
  }
}

TEST_F(ZeroSuggestProviderTest, StartStop) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      "[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX);

  // Make sure valid input starts the provider.
  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to have notified the provider listener.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to have notified the provider listener.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForNTP();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRun) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
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

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestWantAsynchronousMatchesFalse) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  AutocompleteInput input = OnFocusInputForNTP();
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider not to have notified the provider listener.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResults) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX);
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

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestReceivedEmptyResults) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = OnFocusInputForNTP();
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response("[\"\",[],[],[],{}]");
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

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestPrefetchThenNTPOnFocus) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      "[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
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
        GetSuggestURL(metrics::OmniboxEventProto::NTP_ZPS_PREFETCH);
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        "[\"\",[\"search4\", \"search5\", \"search6\"],"
        "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
        "\"google:verbatimrelevance\":1300}]");
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

    // Expect the provider not to have notified the provider listener.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());  // 3 results, no verbatim match

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
    ASSERT_EQ(ZeroSuggestProvider::REMOTE_NO_URL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results from the cached response.
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX);
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        "[\"\",[\"search7\", \"search8\", \"search9\"],"
        "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
        "\"google:verbatimrelevance\":1300}]");
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

    // Expect the provider to have notified the provider listener.
    EXPECT_TRUE(provider_did_notify_);

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
