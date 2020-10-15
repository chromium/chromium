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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/variations/variations_associated_data.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {
class FakeEmptyTopSites : public history::TopSites {
 public:
  FakeEmptyTopSites() {
  }
  FakeEmptyTopSites(const FakeEmptyTopSites&) = delete;
  FakeEmptyTopSites& operator=(const FakeEmptyTopSites&) = delete;

  // history::TopSites:
  void GetMostVisitedURLs(GetMostVisitedURLsCallback callback) override;
  void SyncWithHistory() override {}
  bool HasBlockedUrls() const override { return false; }
  void AddBlockedUrl(const GURL& url) override {}
  void RemoveBlockedUrl(const GURL& url) override {}
  bool IsBlocked(const GURL& url) override { return false; }
  void ClearBlockedUrls() override {}
  bool IsFull() override { return false; }
  bool loaded() const override {
    return false;
  }
  history::PrepopulatedPageList GetPrepopulatedPages() override {
    return history::PrepopulatedPageList();
  }
  void OnNavigationCommitted(const GURL& url) override {}

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override {}

  // Only runs a single callback, so that the test can specify a different
  // set per call.
  void RunACallback(const history::MostVisitedURLList& urls) {
    DCHECK(!callbacks.empty());
    std::move(callbacks.front()).Run(urls);
    callbacks.pop_front();
  }

 protected:
  // A test-specific field for controlling when most visited callback is run
  // after top sites have been requested.
  std::list<GetMostVisitedURLsCallback> callbacks;

  ~FakeEmptyTopSites() override {}
};

void FakeEmptyTopSites::GetMostVisitedURLs(
    GetMostVisitedURLsCallback callback) {
  callbacks.push_back(std::move(callback));
}

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient()
      : template_url_service_(new TemplateURLService(nullptr, 0)),
        top_sites_(new FakeEmptyTopSites()) {
    pref_service_.registry()->RegisterStringPref(
        omnibox::kZeroSuggestCachedResults, std::string());
  }
  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  bool SearchSuggestEnabled() const override { return true; }

  scoped_refptr<history::TopSites> GetTopSites() override { return top_sites_; }

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_.get();
  }

  TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }

  PrefService* GetPrefs() override { return &pref_service_; }

  bool IsPersonalizedUrlDataCollectionActive() const override { return true; }

  void Classify(
      const base::string16& text,
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
  scoped_refptr<history::TopSites> top_sites_;
  TestingPrefServiceSimple pref_service_;
  TestSchemeClassifier scheme_classifier_;
};

}  // namespace

class ZeroSuggestProviderTest : public testing::Test,
                                public AutocompleteProviderListener {
 public:
  ZeroSuggestProviderTest() = default;
  ZeroSuggestProviderTest(const ZeroSuggestProviderTest&) = delete;
  ZeroSuggestProviderTest& operator=(const ZeroSuggestProviderTest&) = delete;

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  void CreateRemoteNoUrlFieldTrial();
  void CreateMostVisitedFieldTrial();
  void SetZeroSuggestVariantForAllContexts(const std::string& variant);

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<ZeroSuggestProvider> provider_;

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
};

void ZeroSuggestProviderTest::SetUp() {
  client_.reset(new FakeAutocompleteProviderClient());

  TemplateURLService* turl_model = client_->GetTemplateURLService();
  turl_model->Load();

  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            turl_model->GetDefaultSearchProvider()->GetEngineType(
                turl_model->search_terms_data()));

  provider_ = ZeroSuggestProvider::Create(client_.get(), this);
}

void ZeroSuggestProviderTest::OnProviderUpdate(bool updated_matches) {
}

void ZeroSuggestProviderTest::CreateRemoteNoUrlFieldTrial() {
  SetZeroSuggestVariantForAllContexts(ZeroSuggestProvider::kRemoteNoUrlVariant);
}

void ZeroSuggestProviderTest::CreateMostVisitedFieldTrial() {
  SetZeroSuggestVariantForAllContexts(ZeroSuggestProvider::kMostVisitedVariant);
}

void ZeroSuggestProviderTest::SetZeroSuggestVariantForAllContexts(
    const std::string& variant) {
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeatureWithParameters(
      omnibox::kOnFocusSuggestions,
      {{std::string(OmniboxFieldTrial::kZeroSuggestVariantRule) + ":*:*",
        variant}});
}

TEST_F(ZeroSuggestProviderTest, AllowZeroSuggestSuggestions) {
  std::string input_url = "https://example.com/";

  AutocompleteInput prefix_input(base::ASCIIToUTF16(input_url),
                                 metrics::OmniboxEventProto::OTHER,
                                 TestSchemeClassifier());
  prefix_input.set_focus_type(OmniboxFocusType::DEFAULT);

  AutocompleteInput on_focus_input(base::ASCIIToUTF16(input_url),
                                   metrics::OmniboxEventProto::OTHER,
                                   TestSchemeClassifier());
  on_focus_input.set_current_url(GURL(input_url));
  on_focus_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput on_clobber_input(base::string16(),
                                     metrics::OmniboxEventProto::OTHER,
                                     TestSchemeClassifier());
  on_clobber_input.set_current_url(GURL(input_url));
  on_clobber_input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);

  // ZeroSuggest should never deal with prefix suggestions.
  EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(prefix_input));

  // This should always be true, as otherwise we will break MostVisited.
  // TODO(tommycli): We should split this into its own provider to avoid
  // breaking it again.
  EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_focus_input));

  EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(on_clobber_input));

  // Enable on-clobber.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        omnibox::kClobberTriggersContextualWebZeroSuggest);
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(prefix_input));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_focus_input));
    EXPECT_TRUE(provider_->AllowZeroSuggestSuggestions(on_clobber_input));

    // Sanity check that we only affect the OTHER page classification.
    AutocompleteInput on_clobber_serp(
        base::string16(),
        metrics::OmniboxEventProto::
            SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT,
        TestSchemeClassifier());
    on_clobber_serp.set_current_url(GURL(input_url));
    on_clobber_serp.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);
    EXPECT_FALSE(provider_->AllowZeroSuggestSuggestions(on_clobber_serp));
  }
}

// TODO(tommycli): Break up this test into smaller ones.
TEST_F(ZeroSuggestProviderTest, TypeOfResultToRun) {
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
#if !defined(OS_ANDROID) && !defined(OS_IOS)  // Desktop
        EXPECT_EQ(BaseSearchProvider::IsNTPPage(current_page_classification) &&
                          remote_no_url_allowed
                      ? ZeroSuggestProvider::ResultType::REMOTE_NO_URL
                      : ZeroSuggestProvider::ResultType::NONE,
                  result_type);
#elif !defined(OS_IOS)  // Android
        EXPECT_EQ(BaseSearchProvider::IsNTPPage(current_page_classification) &&
                          remote_no_url_allowed
                      ? ZeroSuggestProvider::ResultType::REMOTE_NO_URL
                      : !BaseSearchProvider::IsSearchResultsPage(
                            current_page_classification)
                            ? ZeroSuggestProvider::ResultType::MOST_VISITED
                            : ZeroSuggestProvider::ResultType::NONE,
                  result_type);
#else                   // iOS
        EXPECT_EQ(!BaseSearchProvider::IsSearchResultsPage(
                      current_page_classification)
                      ? ZeroSuggestProvider::ResultType::MOST_VISITED
                      : ZeroSuggestProvider::ResultType::NONE,
                  result_type);
#endif
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
  AutocompleteInput ntp_input(base::string16(), metrics::OmniboxEventProto::NTP,
                              TestSchemeClassifier());
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      ntp_input,
      /*remote_no_url_allowed=*/false);

  // Verify RemoteNoUrl works when the user is signed in.
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      ntp_input,
      /*remote_no_url_allowed=*/true);

  CreateRemoteNoUrlFieldTrial();
  GURL other_suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER);
  EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_NO_URL,
            ZeroSuggestProvider::TypeOfResultToRun(client_.get(), other_input,
                                                   other_suggest_url));

  // But if the user has signed out, fall back to platform-specific defaults.
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(false));
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
      /*remote_no_url_allowed=*/false);

  // Unless we allow remote suggestions for signed-out users.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeature(
      omnibox::kOmniboxTrendingZeroPrefixSuggestionsOnNTP);
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
      /*remote_no_url_allowed=*/true);

  // Restore authentication state, but now set a non-Google default search
  // engine. Verify that we still disallow remote suggestions.
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  auto* google_search_provider = turl_model->GetDefaultSearchProvider();
  TemplateURLData data;
  data.SetURL("https://www.example.com/?q={searchTerms}");
  data.suggestions_url = "https://www.example.com/suggest/?q={searchTerms}";
  auto* other_search_provider =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(other_search_provider);
  ExpectPlatformSpecificDefaultZeroSuggestBehavior(
      other_input,
      /*remote_no_url_allowed=*/false);

  // Restore Google as the default search provider.
  turl_model->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_search_provider));

  // Verify a few globally configured states work.
  SetZeroSuggestVariantForAllContexts(
      ZeroSuggestProvider::kRemoteSendUrlVariant);
  EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
            ZeroSuggestProvider::TypeOfResultToRun(client_.get(), other_input,
                                                   other_suggest_url));
  CreateRemoteNoUrlFieldTrial();
  EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_NO_URL,
            ZeroSuggestProvider::TypeOfResultToRun(client_.get(), other_input,
                                                   other_suggest_url));
  CreateMostVisitedFieldTrial();
  EXPECT_EQ(ZeroSuggestProvider::ResultType::MOST_VISITED,
            ZeroSuggestProvider::TypeOfResultToRun(client_.get(), other_input,
                                                   other_suggest_url));

  // Verify that a wildcard rule works in conjunction with a
  // page-classification-specific rule.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitAndEnableFeatureWithParameters(
      omnibox::kOnFocusSuggestions,
      {
          {std::string(OmniboxFieldTrial::kZeroSuggestVariantRule) + ":*:*",
           ZeroSuggestProvider::kMostVisitedVariant},
          {base::StringPrintf("%s:%d:*",
                              OmniboxFieldTrial::kZeroSuggestVariantRule,
                              metrics::OmniboxEventProto::BLANK),
           ZeroSuggestProvider::kNoneVariant},
      });
  EXPECT_EQ(ZeroSuggestProvider::ResultType::MOST_VISITED,
            ZeroSuggestProvider::TypeOfResultToRun(client_.get(), other_input,
                                                   other_suggest_url));

  // Test the BLANK page classification to verify the wildcard rule works.
  {
    std::string url("chrome://newtab/");
    AutocompleteInput blank_input(base::ASCIIToUTF16(url),
                                  metrics::OmniboxEventProto::BLANK,
                                  TestSchemeClassifier());
    blank_input.set_current_url(GURL(url));
    EXPECT_EQ(ZeroSuggestProvider::ResultType::NONE,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), blank_input,
                  GetSuggestURL(metrics::OmniboxEventProto::BLANK)));
  }
}

TEST_F(ZeroSuggestProviderTest, TypeOfResultToRunForContextualWeb) {
  std::string input_url = "https://example.com/";
  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER);

  AutocompleteInput on_focus_input(base::ASCIIToUTF16(input_url),
                                   metrics::OmniboxEventProto::OTHER,
                                   TestSchemeClassifier());
  on_focus_input.set_current_url(GURL(input_url));
  on_focus_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput on_clobber_input(base::string16(),
                                     metrics::OmniboxEventProto::OTHER,
                                     TestSchemeClassifier());
  on_clobber_input.set_current_url(GURL(input_url));
  on_clobber_input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const ZeroSuggestProvider::ResultType kDefaultContextualWebResultType =
      ZeroSuggestProvider::ResultType::MOST_VISITED;
#else
  const ZeroSuggestProvider::ResultType kDefaultContextualWebResultType =
      ZeroSuggestProvider::ResultType::NONE;
#endif

  EXPECT_EQ(kDefaultContextualWebResultType,
            ZeroSuggestProvider::TypeOfResultToRun(
                client_.get(), on_focus_input, suggest_url));
  EXPECT_EQ(kDefaultContextualWebResultType,
            ZeroSuggestProvider::TypeOfResultToRun(
                client_.get(), on_clobber_input, suggest_url));

  // Enable on-focus only.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(omnibox::kOnFocusSuggestionsContextualWeb);

    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input, suggest_url));
    EXPECT_EQ(kDefaultContextualWebResultType,
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
         omnibox::kClobberTriggersContextualWebZeroSuggest},
        {});

    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_focus_input, suggest_url));
    EXPECT_EQ(ZeroSuggestProvider::ResultType::REMOTE_SEND_URL,
              ZeroSuggestProvider::TypeOfResultToRun(
                  client_.get(), on_clobber_input, suggest_url));
  }
}

TEST_F(ZeroSuggestProviderTest, TestDoesNotReturnMatchesForPrefix) {
  CreateRemoteNoUrlFieldTrial();

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false);

  // Expect that matches don't get populated out of cache because we are not
  // in zero suggest mode.
  EXPECT_TRUE(provider_->matches().empty());

  // Expect that loader did not get created.
  EXPECT_EQ(0, test_loader_factory()->NumPending());
}

TEST_F(ZeroSuggestProviderTest, TestStartWillStopForSomeInput) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  CreateRemoteNoUrlFieldTrial();

  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(input_url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);

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

TEST_F(ZeroSuggestProviderTest, TestMostVisitedCallback) {
  CreateMostVisitedFieldTrial();

  std::string current_url("http://www.foxnews.com/");
  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(current_url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  history::MostVisitedURLList urls;
  history::MostVisitedURL url(GURL("http://foo.com/"),
                              base::ASCIIToUTF16("Foo"));
  urls.push_back(url);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  // Should have verbatim match + most visited url match.
  EXPECT_EQ(2U, provider_->matches().size());
  provider_->Stop(false, false);

  provider_->Start(input, false);
  provider_->Stop(false, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Most visited results arriving after Stop() has been called, ensure they
  // are not displayed.
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  EXPECT_TRUE(provider_->matches().empty());

  history::MostVisitedURLList urls2;
  urls2.push_back(history::MostVisitedURL(GURL("http://bar.com/"),
                                          base::ASCIIToUTF16("Bar")));
  urls2.push_back(history::MostVisitedURL(GURL("http://zinga.com/"),
                                          base::ASCIIToUTF16("Zinga")));
  provider_->Start(input, false);
  provider_->Stop(false, false);
  provider_->Start(input, false);
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  // Stale results should get rejected.
  EXPECT_TRUE(provider_->matches().empty());
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls2);
  EXPECT_FALSE(provider_->matches().empty());
  provider_->Stop(false, false);
}

TEST_F(ZeroSuggestProviderTest, TestMostVisitedNavigateToSearchPage) {
  CreateMostVisitedFieldTrial();

  std::string current_url("http://www.foxnews.com/");
  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(current_url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  history::MostVisitedURLList urls;
  history::MostVisitedURL url(GURL("http://foo.com/"),
                              base::ASCIIToUTF16("Foo"));
  urls.push_back(url);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Stop() doesn't always get called.

  std::string search_url("https://www.google.com/?q=flowers");
  AutocompleteInput srp_input(
      base::ASCIIToUTF16(search_url),
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      TestSchemeClassifier());
  srp_input.set_current_url(GURL(search_url));
  srp_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  provider_->Start(srp_input, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Most visited results arriving after a new request has been started.
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRun) {
  CreateRemoteNoUrlFieldTrial();
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  provider_->Start(input, false);

  EXPECT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();

#if defined(OS_ANDROID) || defined(OS_IOS)
  EXPECT_EQ(4U, provider_->matches().size());  // 3 results + verbatim
#else
  EXPECT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
#endif

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching)) {
    EXPECT_EQ(json_response,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
  }
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResults) {
  CreateRemoteNoUrlFieldTrial();
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false);

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching)) {
    // Expect that matches get populated synchronously out of the cache.
#if defined(OS_ANDROID) || defined(OS_IOS)
    ASSERT_EQ(4U, provider_->matches().size());  // 3 results + verbatim
    EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[2].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[3].contents);
#else
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[0].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[2].contents);
#endif
  }

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2("[\"\",[\"search4\", \"search5\", \"search6\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching)) {
    // Expect the same results after the response has been handled.
#if defined(OS_ANDROID) || defined(OS_IOS)
    ASSERT_EQ(4U, provider_->matches().size());  // 3 results + verbatim
    EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[2].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[3].contents);
#else
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[0].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[2].contents);
#endif

  // Expect the new results have been stored.
  EXPECT_EQ(json_response2,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
  } else {
    // Expect fresh results after the response has been handled.
#if defined(OS_ANDROID) || defined(OS_IOS)
    ASSERT_EQ(4U, provider_->matches().size());  // 3 results + verbatim
    EXPECT_EQ(base::ASCIIToUTF16("search4"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search5"), provider_->matches()[2].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search6"), provider_->matches()[3].contents);
#else
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(base::ASCIIToUTF16("search4"), provider_->matches()[0].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search5"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search6"), provider_->matches()[2].contents);
#endif
  }
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestReceivedEmptyResults) {
  CreateRemoteNoUrlFieldTrial();
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false);

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching)) {
    // Expect that matches get populated synchronously out of the cache.
#if defined(OS_ANDROID) || defined(OS_IOS)
    ASSERT_EQ(4U, provider_->matches().size());  // 3 results + verbatim
    EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[2].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[3].contents);
#else
    ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
    EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[0].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[1].contents);
    EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[2].contents);
#endif
  } else {
    ASSERT_TRUE(provider_->matches().empty());
  }

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER);
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response("[\"\",[],[],[],{}]");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching)) {
    // Expect the new results have been stored.
    EXPECT_EQ(empty_response,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
  }
}
