// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {
#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<TemplateURLData> GenerateSimpleTemplateURLData(
    const std::string& keyword) {
  auto data = std::make_unique<TemplateURLData>();
  data->SetShortName(base::UTF8ToUTF16(keyword));
  data->SetKeyword(base::UTF8ToUTF16(keyword));
  data->SetURL(std::string("https://") + keyword + "/q={searchTerms}");
  return data;
}
#endif

using testing::_;

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryURL,
              (const GURL& url,
               bool want_visits,
               history::HistoryService::QueryURLCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
};
}  // namespace

class ZeroSuggestVerbatimMatchProviderTest
    : public testing::TestWithParam<
          metrics::OmniboxEventProto::PageClassification> {
 public:
  ZeroSuggestVerbatimMatchProviderTest() = default;
  void SetUp() override;

 protected:
  bool IsVerbatimMatchEligible() const;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  FakeAutocompleteProviderClient mock_client_;
  scoped_refptr<ZeroSuggestVerbatimMatchProvider> provider_;
};

bool ZeroSuggestVerbatimMatchProviderTest::IsVerbatimMatchEligible() const {
  using OEP = metrics::OmniboxEventProto;

  auto param = GetParam();
  return param == OEP::OTHER || param == OEP::OTHER_ON_CCT ||
         param == OEP::SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT ||
         param == OEP::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT ||
         param == OEP::SEARCH_RESULT_PAGE_ON_CCT;
}

void ZeroSuggestVerbatimMatchProviderTest::SetUp() {
  provider_ = new ZeroSuggestVerbatimMatchProvider(&mock_client_);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return false; });
  ON_CALL(mock_client_, Classify)
      .WillByDefault(
          [](const std::u16string& text, bool prefer_keyword,
             bool allow_exact_keyword_match,
             metrics::OmniboxEventProto::PageClassification page_classification,
             AutocompleteMatch* match,
             GURL* alternate_nav_url) { match->destination_url = GURL(text); });
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWithUserTextInOmnibox) {
  std::string query("user input");
  std::string url("https://google.com/search?q=test");
  AutocompleteInput input(base::ASCIIToUTF16(query), GetParam(),
                          TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  provider_->Start(input, false);

  // Clobber state should never generate a verbatim match.
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWithUserTextInOmniboxInIncognito) {
  std::string query("user input");
  std::string url("https://google.com/search?q=test");
  AutocompleteInput input(base::ASCIIToUTF16(query), GetParam(),
                          TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);

  // Clobber state should never generate a verbatim match.
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest, OffersVerbatimMatchOnFocus) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), GetParam(),
                          TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  ASSERT_EQ(IsVerbatimMatchEligible(), provider_->matches().size() > 0);
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       OffersVerbatimMatchOnFocusInIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), GetParam(),
                          TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);
  ASSERT_EQ(IsVerbatimMatchEligible(), provider_->matches().size() > 0);
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest, NoVerbatimMatchWithEmptyInput) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  provider_->Start(input, false);
  ASSERT_TRUE(provider_->matches().empty());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWithEmptyInputInIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);
  ASSERT_TRUE(provider_->matches().empty());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest, OffersVerbatimMatchOnClobber) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  provider_->Start(input, false);
  ASSERT_EQ(IsVerbatimMatchEligible(), provider_->matches().size() > 0);
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       OffersVerbatimMatchOnClobberInIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);
  ASSERT_EQ(IsVerbatimMatchEligible(), provider_->matches().size() > 0);
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

#if !BUILDFLAG(IS_ANDROID)
TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       DoesNotAttemptToPopulateFillIntoEditWithFeatureDisabled) {
  base::test::ScopedFeatureList features;
  // Clear the TemplateURLService. Observe crash, if we attempt to use it.
  mock_client_.set_template_url_service(nullptr);

  std::string url("https://www.wider.com/");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  provider_->Start(input, false);
  if (IsVerbatimMatchEligible()) {
    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wider.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"title", provider_->matches()[0].description);
  }
}
#endif

#if BUILDFLAG(IS_ANDROID)
TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoFillIntoEditResolutionWithNoSearchEngines) {
  base::test::ScopedFeatureList features;
  std::string url("https://www.search.com/q=abc");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  provider_->Start(input, false);
  if (IsVerbatimMatchEligible()) {
    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.search.com/q=abc",
              provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"title", provider_->matches()[0].description);
  }
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       UpdateFillIntoEditWhenUrlMatchesSearchResultsPage) {
  base::test::ScopedFeatureList features;

  // Default TemplateURL to parse the URL.
  std::unique_ptr<TemplateURLData> engine =
      GenerateSimpleTemplateURLData("www.search.com");
  mock_client_.GetTemplateURLService()->ApplyDefaultSearchChangeForTesting(
      engine.get(), DefaultSearchManager::FROM_USER);

  std::string url("https://www.search.com/q=abc");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  provider_->Start(input, false);
  if (IsVerbatimMatchEligible()) {
    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"abc", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"title", provider_->matches()[0].description);
  }
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       DontUpdateFillIntoEditWhenUrlMatchesNonDefaultSearchEngine) {
  base::test::ScopedFeatureList features;

  // Default TemplateURL to parse the URL.
  std::unique_ptr<TemplateURLData> engine =
      GenerateSimpleTemplateURLData("www.search.com");
  // Other search engines.
  TemplateURLService::Initializer other_engines[] = {
      {"non-default", "https://www.non-default.com/q=abc", "non-default"}};
  search_engines::SearchEnginesTestEnvironment test_environment(
      {.template_url_service_initializer = other_engines});
  mock_client_.set_template_url_service(
      test_environment.template_url_service());
  mock_client_.GetTemplateURLService()->ApplyDefaultSearchChangeForTesting(
      engine.get(), DefaultSearchManager::FROM_USER);

  std::string url("https://www.non-default.com/q=abc");
  AutocompleteInput input(std::u16string(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_title(u"title");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  provider_->Start(input, false);
  if (IsVerbatimMatchEligible()) {
    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.non-default.com/q=abc",
              provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"title", provider_->matches()[0].description);
  }

  // `mock_client_` points to the `TemplateURLService` found in
  // `test_environment`, which is going out of scope here.
  // Destroy it to avoid dangling pointers.
  mock_client_.set_template_url_service(nullptr);
}
#endif

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       MissingPageTitle_NoHistoryService) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(u"",  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  // Note: no page title.
  input.set_current_title(u"");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
  provider_->Start(input, false);

  if (IsVerbatimMatchEligible()) {
    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wired.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"", provider_->matches()[0].description);
  }
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       MissingPageTitle_WithHistoryService) {
  // Install history service mock.
  auto mock_service_up = std::make_unique<MockHistoryService>();
  auto& mock_service = *mock_service_up;
  mock_client_.set_history_service(std::move(mock_service_up));

  std::string url("https://www.wired.com/");
  AutocompleteInput input(u"",  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  // Note: no page title.
  input.set_current_title(u"");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);

  if (!IsVerbatimMatchEligible()) {
    return;
  }

  history::HistoryService::QueryURLCallback callback;
  {
    EXPECT_CALL(mock_service, QueryURL(_, _, _, _))
        .WillOnce([&](GURL url, bool want_visits,
                      history::HistoryService::QueryURLCallback cb,
                      base::CancelableTaskTracker* tracker)
                      -> base::CancelableTaskTracker::TaskId {
          EXPECT_EQ("https://www.wired.com/", url.spec());
          EXPECT_FALSE(want_visits);
          callback = std::move(cb);
          return {};
        });
    provider_->Start(input, false);

    // Default matches with no title.
    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wired.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"", provider_->matches()[0].description);
  }

  {
    // Resolve history service.
    history::QueryURLResult history_row;
    history_row.row.set_title(u"Testing");
    std::move(callback).Run(std::move(history_row));

    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wired.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"Testing", provider_->matches()[0].description);
  }
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       MissingPageTitle_WithHistoryService_Synchronous) {
  // Install history service mock.
  auto mock_service_up = std::make_unique<MockHistoryService>();
  auto& mock_service = *mock_service_up;
  mock_client_.set_history_service(std::move(mock_service_up));

  std::string url("https://www.wired.com/");
  AutocompleteInput input(u"",  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  // Note: no page title.
  input.set_current_title(u"");
  input.set_omit_asynchronous_matches(true);
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);

  if (!IsVerbatimMatchEligible()) {
    return;
  }

  {
    EXPECT_CALL(mock_service, QueryURL(_, _, _, _)).Times(0);
    provider_->Start(input, false);

    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wired.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"", provider_->matches()[0].description);
  }
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       MissingPageTitle_CallbackCanceled) {
  // Install history service mock.
  auto mock_service_up = std::make_unique<MockHistoryService>();
  auto& mock_service = *mock_service_up;
  mock_client_.set_history_service(std::move(mock_service_up));

  std::string url("https://www.wired.com/");
  AutocompleteInput input(u"",  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  // Note: no page title.
  input.set_current_title(u"");
  input.set_current_url(GURL(url));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);

  if (!IsVerbatimMatchEligible()) {
    return;
  }

  history::HistoryService::QueryURLCallback callback;
  {
    EXPECT_CALL(mock_service, QueryURL(_, _, _, _))
        .WillOnce([&](GURL url, bool want_visits,
                      history::HistoryService::QueryURLCallback cb,
                      base::CancelableTaskTracker* tracker)
                      -> base::CancelableTaskTracker::TaskId {
          EXPECT_EQ("https://www.wired.com/", url.spec());
          EXPECT_FALSE(want_visits);
          callback = std::move(cb);
          return {};
        });
    provider_->Start(input, false);

    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wired.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"", provider_->matches()[0].description);
  }

  // Cancel action.
  provider_->Stop(false, false);

  {
    // Resolve history service.
    // Expect NO AutocompleteMatch.
    history::QueryURLResult history_row;
    history_row.row.set_title(u"Testing");
    std::move(callback).Run(std::move(history_row));

    ASSERT_FALSE(provider_->matches().empty());
    ASSERT_EQ(u"https://www.wired.com", provider_->matches()[0].fill_into_edit);
    ASSERT_EQ(u"", provider_->matches()[0].description);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ZeroSuggestVerbatimMatchProviderNonIncognitoTests,
    ZeroSuggestVerbatimMatchProviderTest,
    ::testing::Values(
        // Variants that should offer verbatim match.
        metrics::OmniboxEventProto::OTHER,
        metrics::OmniboxEventProto::
            SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT,
        metrics::OmniboxEventProto::
            SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,

        // Variants that should offer no verbatim match.
        metrics::OmniboxEventProto::NTP,
        metrics::OmniboxEventProto::BLANK,
        metrics::OmniboxEventProto::HOME_PAGE,
        metrics::OmniboxEventProto::OTHER_ON_CCT,
        metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_ON_CCT,
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS),

    // Ensure clarity when error message is printed out.
    +[](const ::testing::TestParamInfo<
         metrics::OmniboxEventProto::PageClassification> context)
        -> std::string {
      return metrics::OmniboxEventProto::PageClassification_Name(context.param);
    });
