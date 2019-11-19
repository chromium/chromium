// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Invoke;

class MockSearchSuggestService : public SearchSuggestService {
 public:
  MOCK_METHOD0(Refresh, void());

  void RefreshImpl() {
    SearchSuggestDataLoaded(search_suggest_status_, search_suggest_data_);
  }

  explicit MockSearchSuggestService(Profile* profile)
      : SearchSuggestService(profile, nullptr, nullptr) {}

  void set_search_suggest_data(const SearchSuggestData& search_suggest_data) {
    search_suggest_data_ = search_suggest_data;
  }

  void set_search_suggest_status(
      const SearchSuggestLoader::Status search_suggest_status) {
    search_suggest_status_ = search_suggest_status;
  }

  void SuggestionsDisplayed() override { impression_count_++; }

  int impression_count() { return impression_count_; }

  const base::Optional<SearchSuggestData>& search_suggest_data()
      const override {
    return search_suggest_data_;
  }

  const SearchSuggestLoader::Status& search_suggest_status() const override {
    return search_suggest_status_;
  }

 private:
  int impression_count_ = 0;
  base::Optional<SearchSuggestData> search_suggest_data_;
  SearchSuggestLoader::Status search_suggest_status_;
};

class LocalNTPSearchSuggestTest : public InProcessBrowserTest {
 protected:
  MockSearchSuggestService* search_suggest_service() {
    return static_cast<MockSearchSuggestService*>(
        SearchSuggestServiceFactory::GetForProfile(browser()->profile()));
  }

 private:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(&LocalNTPSearchSuggestTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateSearchSuggestService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<MockSearchSuggestService>(profile);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SearchSuggestServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &LocalNTPSearchSuggestTest::CreateSearchSuggestService));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPSearchSuggestTest,
                       SuggestionsInjectedIntoPageEnUS) {
  EXPECT_EQ(base::nullopt, search_suggest_service()->search_suggest_data());

  OneGoogleBarService* one_google_bar_service =
      OneGoogleBarServiceFactory::GetForProfile(browser()->profile());
  one_google_bar_service->SetLanguageCodeForTesting("en-US");

  base::HistogramTester histograms;

  SearchSuggestData data;
  data.suggestions_html = "<div>suggestions</div>";
  data.end_of_body_script = "console.log('suggestions-done')";
  search_suggest_service()->set_search_suggest_data(data);
  search_suggest_service()->set_search_suggest_status(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS);

  EXPECT_CALL(*search_suggest_service(), Refresh())
      .WillOnce(Invoke(search_suggest_service(),
                       &MockSearchSuggestService::RefreshImpl));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab,
                                                    "suggestions-done");
  active_tab->SetDelegate(&console_observer);
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);
  console_observer.Wait();
  EXPECT_EQ("suggestions-done", console_observer.message());

  bool result;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('suggestions').innerHTML === '<div>suggestions</div>'",
      &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(1, search_suggest_service()->impression_count());

  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.SuccessWithSuggestions",
      1);
  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.SuccessWithoutSuggestions",
      0);
  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.SearchSuggestions.RequestStatusV2",
                              1);
}

IN_PROC_BROWSER_TEST_F(
    LocalNTPSearchSuggestTest,
    NoSuggestionsjectedIntoPageOnResponseWithoutSuggestions) {
  EXPECT_EQ(base::nullopt, search_suggest_service()->search_suggest_data());

  base::HistogramTester histograms;
  SearchSuggestData data;
  search_suggest_service()->set_search_suggest_data(data);
  search_suggest_service()->set_search_suggest_status(
      SearchSuggestLoader::Status::OK_WITHOUT_SUGGESTIONS);

  OneGoogleBarService* one_google_bar_service =
      OneGoogleBarServiceFactory::GetForProfile(browser()->profile());
  one_google_bar_service->SetLanguageCodeForTesting("en-US");

  EXPECT_CALL(*search_suggest_service(), Refresh())
      .WillOnce(Invoke(search_suggest_service(),
                       &MockSearchSuggestService::RefreshImpl));

  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('suggestions') === null", &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, search_suggest_service()->impression_count());

  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.SuccessWithSuggestions",
      0);
  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.SuccessWithoutSuggestions",
      1);
  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.SearchSuggestions.RequestStatusV2",
                              1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPSearchSuggestTest,
                       SuggestionsNotInjectedIntoPageNonEnUS) {
  EXPECT_EQ(base::nullopt, search_suggest_service()->search_suggest_data());
  search_suggest_service()->set_search_suggest_status(
      SearchSuggestLoader::Status::FATAL_ERROR);
  search_suggest_service()->Refresh();

  base::HistogramTester histograms;

  OneGoogleBarService* one_google_bar_service =
      OneGoogleBarServiceFactory::GetForProfile(browser()->profile());
  one_google_bar_service->SetLanguageCodeForTesting("en-UK");

  SearchSuggestData data;
  data.suggestions_html = "<div>suggestions</div>";
  data.end_of_body_script = "console.log('suggestions-done')";
  search_suggest_service()->set_search_suggest_data(data);

  EXPECT_CALL(*search_suggest_service(), Refresh()).Times(0);

  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);
  bool result;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('suggestions') === null", &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, search_suggest_service()->impression_count());
}

IN_PROC_BROWSER_TEST_F(LocalNTPSearchSuggestTest,
                       EmptySuggestionsNotInjectedIntoPage) {
  EXPECT_EQ(base::nullopt, search_suggest_service()->search_suggest_data());

  base::HistogramTester histograms;

  OneGoogleBarService* one_google_bar_service =
      OneGoogleBarServiceFactory::GetForProfile(browser()->profile());
  one_google_bar_service->SetLanguageCodeForTesting("en-US");

  SearchSuggestData data;
  search_suggest_service()->set_search_suggest_data(data);
  search_suggest_service()->set_search_suggest_status(
      SearchSuggestLoader::Status::OK_WITHOUT_SUGGESTIONS);
  EXPECT_CALL(*search_suggest_service(), Refresh())
      .WillOnce(Invoke(search_suggest_service(),
                       &MockSearchSuggestService::RefreshImpl));

  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('suggestions') === null", &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, search_suggest_service()->impression_count());

  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.SuccessWithSuggestions",
      0);
  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.SuccessWithoutSuggestions",
      1);
  histograms.ExpectTotalCount(
      "NewTabPage.SearchSuggestions.RequestLatencyV2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.SearchSuggestions.RequestStatusV2",
                              1);
}
