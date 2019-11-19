// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ntp_snippets::Category;
using ntp_snippets::ContentSuggestion;

namespace offline_pages {

namespace {

const base::string16 kTestTitle = base::ASCIIToUTF16("Title 1");

ContentSuggestion ContentSuggestionFromTestURL(const GURL& test_url) {
  auto category =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);
  ContentSuggestion suggestion =
      ContentSuggestion(category, test_url.spec(), test_url);
  suggestion.set_title(kTestTitle);
  return suggestion;
}

}  // namespace

class OfflinePageSuggestedArticlesObserverTest : public testing::Test {
 public:
  OfflinePageSuggestedArticlesObserverTest() {
    prefetch_service_test_taco_ = std::make_unique<PrefetchServiceTestTaco>();
    test_prefetch_dispatcher_ = new TestPrefetchDispatcher();
    prefetch_service_test_taco_->SetPrefetchDispatcher(
        base::WrapUnique(test_prefetch_dispatcher_));
    prefetch_service_test_taco_->SetSuggestedArticlesObserver(
        std::make_unique<SuggestedArticlesObserver>());
    prefetch_service_test_taco_->CreatePrefetchService();
  }

  ~OfflinePageSuggestedArticlesObserverTest() override {
    // Ensure the store can be properly disposed off.
    prefetch_service_test_taco_.reset();
    task_environment_.RunUntilIdle();
  }

  SuggestedArticlesObserver* observer() {
    return prefetch_service_test_taco_->prefetch_service()
        ->GetSuggestedArticlesObserverForTesting();
  }

  TestPrefetchDispatcher* test_prefetch_dispatcher() {
    return test_prefetch_dispatcher_;
  }

 protected:
  Category category =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<PrefetchServiceTestTaco> prefetch_service_test_taco_;

  // Owned by the PrefetchServiceTestTaco.
  TestPrefetchDispatcher* test_prefetch_dispatcher_;
};

TEST_F(OfflinePageSuggestedArticlesObserverTest,
       ForwardsSuggestionsToPrefetchService) {
  const GURL test_url_1("https://www.example.com/1");
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_1));

  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  EXPECT_EQ(1, test_prefetch_dispatcher()->new_suggestions_count);
  EXPECT_EQ(1U, test_prefetch_dispatcher()->latest_prefetch_urls.size());
  EXPECT_EQ(test_url_1,
            test_prefetch_dispatcher()->latest_prefetch_urls[0].url);
  EXPECT_EQ(kTestTitle,
            test_prefetch_dispatcher()->latest_prefetch_urls[0].title);
  EXPECT_EQ(kSuggestedArticlesNamespace,
            test_prefetch_dispatcher()->latest_name_space);
}

TEST_F(OfflinePageSuggestedArticlesObserverTest, RemovesAllOnBadStatus) {
  const GURL test_url_1("https://www.example.com/1");
  const GURL test_url_2("https://www.example.com/2");
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_1));
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_2));

  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  ASSERT_EQ(2U, test_prefetch_dispatcher()->latest_prefetch_urls.size());

  observer()->OnCategoryStatusChanged(
      category, ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
  EXPECT_EQ(1, test_prefetch_dispatcher()->remove_all_suggestions_count);
  observer()->OnCategoryStatusChanged(
      category,
      ntp_snippets::CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
  EXPECT_EQ(2, test_prefetch_dispatcher()->remove_all_suggestions_count);
}

TEST_F(OfflinePageSuggestedArticlesObserverTest, RemovesClientIdOnInvalidated) {
  const GURL test_url_1("https://www.example.com/1");
  observer()->GetTestingArticles()->push_back(
      ContentSuggestionFromTestURL(test_url_1));
  observer()->OnCategoryStatusChanged(category,
                                      ntp_snippets::CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(category);
  ASSERT_EQ(1U, test_prefetch_dispatcher()->latest_prefetch_urls.size());

  observer()->OnSuggestionInvalidated(
      ntp_snippets::ContentSuggestion::ID(category, test_url_1.spec()));

  EXPECT_EQ(1, test_prefetch_dispatcher()->remove_by_client_id_count);
  EXPECT_NE(nullptr, test_prefetch_dispatcher()->last_removed_client_id.get());
  EXPECT_EQ(test_url_1.spec(),
            test_prefetch_dispatcher()->last_removed_client_id->id);
  EXPECT_EQ(kSuggestedArticlesNamespace,
            test_prefetch_dispatcher()->latest_name_space);
}

}  // namespace offline_pages
