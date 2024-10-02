// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/local_history_zero_suggest_provider.h"

#include <limits>
#include <memory>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/keyword_search_term_util.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

using base::Time;
using metrics::OmniboxEventProto;
using OmniboxFieldTrial::kLocalHistoryZeroSuggestRelevanceScore;

namespace {

// Used to populate the URLDatabase.
struct TestURLData {
  raw_ptr<const TemplateURL> search_provider;
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
    : public testing::TestWithParam<bool>,
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
    identity_env_ = std::make_unique<signin::IdentityTestEnvironment>(
        &test_url_loader_factory_);

    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    client_->set_identity_manager(identity_env_->identity_manager());
    CHECK(history_dir_.CreateUniqueTempDir());
    client_->set_history_service(
        history::CreateHistoryService(history_dir_.GetPath(), true));
    client_->set_bookmark_model(bookmarks::TestBookmarkClient::CreateModel());

    provider_ = base::WrapRefCounted(
        LocalHistoryZeroSuggestProvider::Create(client_.get(), this));

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
    task_environment_.RunUntilIdle();
  }

  // AutocompleteProviderListener
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  // Fills the URLDatabase with search URLs using the provided information.
  void LoadURLs(const std::vector<TestURLData>& url_data_list);

  // Creates an input using the provided information and queries the provider.
  void StartProviderAndWaitUntilDone(const std::string& text,
                                     metrics::OmniboxFocusType focus_type,
                                     PageClassification page_classification,
                                     const std::string& current_url);

  // Verifies that provider matches are as expected.
  void ExpectMatches(const std::vector<TestMatchData>& match_data_list);

  // Makes an "unconsented" primary account available.
  void SignIn();

  // Clears the primary account.
  void SignOut();

  const TemplateURL* default_search_provider() {
    return client_->GetTemplateURLService()->GetDefaultSearchProvider();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  // Used to spin the message loop until |provider_| is done with its async ops.
  std::unique_ptr<base::RunLoop> provider_run_loop_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_env_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<LocalHistoryZeroSuggestProvider> provider_;
};

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
        entry.typed_count, now - base::Seconds(entry.age_in_seconds),
        entry.hidden, history::SOURCE_BROWSED);
    client_->GetHistoryService()->SetKeywordSearchTermsForURL(
        GURL(search_url), entry.search_provider->id(),
        base::UTF8ToUTF16(entry.search_terms));
    history::BlockUntilHistoryProcessesPendingRequests(
        client_->GetHistoryService());
  }
}

void LocalHistoryZeroSuggestProviderTest::StartProviderAndWaitUntilDone(
    const std::string& text = "",
    metrics::OmniboxFocusType focus_type =
        metrics::OmniboxFocusType::INTERACTION_FOCUS,
    PageClassification page_classification = OmniboxEventProto::NTP_REALBOX,
    const std::string& current_url = "") {
  AutocompleteInput input(base::ASCIIToUTF16(text), page_classification,
                          TestSchemeClassifier());
  input.set_focus_type(focus_type);
  input.set_current_url(GURL(current_url));
  provider_->Start(input, false);
  if (!provider_->done()) {
    provider_run_loop_ = std::make_unique<base::RunLoop>();
    // Quits in OnProviderUpdate when the provider is done.
    provider_run_loop_->Run();
  }
}

void LocalHistoryZeroSuggestProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
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

void LocalHistoryZeroSuggestProviderTest::SignIn() {
  identity_env_->MakePrimaryAccountAvailable("test@email.com",
                                             signin::ConsentLevel::kSignin);
}

void LocalHistoryZeroSuggestProviderTest::SignOut() {
  identity_env_->ClearPrimaryAccount();
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
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTimeV2", 0);

  StartProviderAndWaitUntilDone(/*text=*/"",
                                metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  ExpectMatches({});

  // Following histograms should not be logged if zero-prefix suggestions are
  // not allowed.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTimeV2", 0);

  StartProviderAndWaitUntilDone();
  ExpectMatches(
      {{"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get()}});

  // Following histograms should be logged when zero-prefix suggestions are
  // allowed and the keyword search terms database is queried.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTimeV2", 1);
  // Deletion histograms should not be logged unless a suggestion is deleted.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SyncDeleteTime", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime", 0);
}

// Tests that suggestions are not returned in an off-the-record context.
TEST_F(LocalHistoryZeroSuggestProviderTest, Incognito) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  EXPECT_CALL(*client_.get(), IsOffTheRecord())
      .WillRepeatedly(testing::Return(true));

  StartProviderAndWaitUntilDone();
  ExpectMatches({});
}

// Tests that suggestions are returned in a non off-the-record context.
TEST_F(LocalHistoryZeroSuggestProviderTest, NonIncognito) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  EXPECT_CALL(*client_.get(), IsOffTheRecord())
      .WillRepeatedly(testing::Return(false));

  StartProviderAndWaitUntilDone();
  ExpectMatches(
      {{"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get()}});
}

// Tests that suggestions are allowed in the eligibile entry points.
TEST_F(LocalHistoryZeroSuggestProviderTest, EntryPoint) {
  LoadURLs({
      {default_search_provider(), "hello world", "&foo=bar", 1},
  });

  {
    // Disable local history zero-prefix suggestions beyond NTP.
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(omnibox::kLocalHistoryZeroSuggestBeyondNTP);
    StartProviderAndWaitUntilDone();

    // Local history zero-prefix suggestions are enabled by default.
    ExpectMatches(
        {{"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get()}});
  }
  {
    // Disable local history zero-prefix suggestions beyond NTP.
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{omnibox::kLocalHistoryZeroSuggestBeyondNTP});
    StartProviderAndWaitUntilDone(
        /*text=*/"https://example.com/",
        metrics::OmniboxFocusType::INTERACTION_FOCUS,
        OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
        /*current_url=*/"https://example.com/");

    // Local history zero-prefix suggestions are disabled for on-focus SRP.
    ExpectMatches({});
  }
  {
    // Enable local history zero-prefix suggestions beyond NTP.
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/
        {
            omnibox::kLocalHistoryZeroSuggestBeyondNTP,
        },
        /*disabled_features=*/{});
    StartProviderAndWaitUntilDone(
        /*text=*/"https://example.com/",
        metrics::OmniboxFocusType::INTERACTION_FOCUS,
        OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
        /*current_url=*/"https://example.com/");

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    // Local history zero-prefix suggestions are enabled for on-focus SRP.
    ExpectMatches(
        {{"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get()}});
#else
    // Desktop does not support that.
    ExpectMatches({});
#endif
  }
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  {
    // Enable local history zero-prefix suggestions beyond NTP.
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{omnibox::kLocalHistoryZeroSuggestBeyondNTP},
        /*disabled_features=*/{});
    StartProviderAndWaitUntilDone(
        /*text=*/"https://example.com/",
        metrics::OmniboxFocusType::INTERACTION_FOCUS,
        OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
        /*current_url=*/"https://example.com/");

    // Local history zero-prefix suggestions are disabled for on-focus SRP.
    ExpectMatches({});
  }
#endif
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
  ExpectMatches(
      {{"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get()}});

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
  ExpectMatches(
      {{"سلام دنیا", kLocalHistoryZeroSuggestRelevanceScore.Get()},
       {"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get() - 1}});
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

  // More recent searches are ranked higher when searches are just as frequent.
  StartProviderAndWaitUntilDone();
  ExpectMatches(
      {{"more recent search", kLocalHistoryZeroSuggestRelevanceScore.Get()},
       {"less recent search",
        kLocalHistoryZeroSuggestRelevanceScore.Get() - 1}});

  // More frequent searches are ranked higher when searches are nearly as old.
  LoadURLs({
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "less recent search", "&foo=bar4",
       /*age_in_seconds=*/2, /*visit_count=*/5},
  });

  StartProviderAndWaitUntilDone();
  ExpectMatches(
      {{"less recent search", kLocalHistoryZeroSuggestRelevanceScore.Get()},
       {"more recent search",
        kLocalHistoryZeroSuggestRelevanceScore.Get() - 1}});
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
  ExpectMatches({{"hello world", kLocalHistoryZeroSuggestRelevanceScore.Get()},
                 {"not to be deleted",
                  kLocalHistoryZeroSuggestRelevanceScore.Get() - 1}});

  // The keyword search terms database should be queried for the search terms
  // submitted to the default search provider.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SearchTermsExtractionTimeV2", 1);

  provider_->DeleteMatch(provider_->matches()[0]);

  // Histogram tracking the synchronous deletion duration should get logged
  // synchronously.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.SyncDeleteTime", 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime", 0);

  // Make sure the deletion takes effect immediately in the provider before the
  // history service asynchronously performs the deletion or even before the
  // provider is started again.
  ExpectMatches({{"not to be deleted",
                  kLocalHistoryZeroSuggestRelevanceScore.Get() - 1}});

  StartProviderAndWaitUntilDone();
  ExpectMatches(
      {{"not to be deleted", kLocalHistoryZeroSuggestRelevanceScore.Get()}});

  // Wait until the history service performs the deletion.
  history::BlockUntilHistoryProcessesPendingRequests(
      client_->GetHistoryService());

  // Histogram tracking the async deletion duration should get logged once the
  // HistoryService async task returns to the initiating thread.
  histogram_tester.ExpectTotalCount(
      "Omnibox.LocalHistoryZeroSuggest.AsyncDeleteTime", 1);

  StartProviderAndWaitUntilDone();
  ExpectMatches(
      {{"not to be deleted", kLocalHistoryZeroSuggestRelevanceScore.Get()}});

  history::URLDatabase* url_db =
      client_->GetHistoryService()->InMemoryDatabase();

  // Make sure all the search terms for the default search provider that would
  // produce the deleted match are deleted.
  std::vector<std::unique_ptr<history::KeywordSearchTermVisit>> visits;
  auto enumerator_1 = url_db->CreateKeywordSearchTermVisitEnumerator(
      default_search_provider()->id());
  ASSERT_TRUE(enumerator_1);
  history::GetAutocompleteSearchTermsFromEnumerator(
      *enumerator_1, /*count=*/SIZE_MAX,
      history::SearchTermRankingPolicy::kFrecency, &visits);
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(u"not to be deleted", visits[0]->normalized_term);

  // Make sure search terms from other search providers that would produce the
  // deleted match are not deleted.
  visits.clear();
  auto enumerator_2 = url_db->CreateKeywordSearchTermVisitEnumerator(
      other_search_provider->id());
  ASSERT_TRUE(enumerator_2);
  history::GetAutocompleteSearchTermsFromEnumerator(
      *enumerator_2, /*count=*/SIZE_MAX,
      history::SearchTermRankingPolicy::kFrecency, &visits);
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(u"hello world", visits[0]->normalized_term);
}
