// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/content_metadata.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/core/prefetch/stub_prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace feed {

namespace {

using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModel;
using offline_pages::PageCriteria;
using offline_pages::PrefetchSuggestion;
using offline_pages::StubOfflinePageModel;
using offline_pages::StubPrefetchService;
using offline_pages::SuggestionsProvider;

constexpr char kUrl1[] = "https://www.one.com/";
constexpr char kUrl2[] = "https://www.two.com/";
constexpr char kUrl3[] = "https://www.three.com/";

constexpr char kOne[] = "One";
constexpr char kTwo[] = "Two";
constexpr char kThree[] = "Three";

MATCHER_P(EqualsSpec, expected, "") {
  return arg.spec() == expected;
}

class TestOfflinePageModel : public StubOfflinePageModel {
 public:
  void AddOfflinedPage(const std::string& url,
                       const std::string& original_url,
                       int64_t offline_id,
                       base::Time creation_time,
                       std::string name_space) {
    OfflinePageItem item;
    item.url = GURL(url);
    item.original_url_if_different = GURL(original_url);
    item.offline_id = offline_id;
    item.creation_time = creation_time;
    item.client_id = offline_pages::ClientId(name_space, "");
    url_to_offline_page_item_.emplace(url, item);
    if (!original_url.empty()) {
      url_to_offline_page_item_.emplace(original_url, item);
    }
  }

  void AddOfflinedPage(const std::string& url, int64_t offline_id) {
    AddOfflinedPage(url, "", offline_id, base::Time(), "");
  }

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));

 private:
  void GetPagesWithCriteria(const PageCriteria& criteria,
                            MultipleOfflinePageItemCallback callback) override {
    // Feed should ignore tab-bound pages.
    EXPECT_TRUE(criteria.exclude_tab_bound_pages);

    auto iter = url_to_offline_page_item_.equal_range(criteria.url.spec());
    std::vector<OfflinePageItem> ret;
    ret.resize(std::distance(iter.first, iter.second));
    std::transform(iter.first, iter.second, ret.begin(),
                   [](auto pair) { return pair.second; });

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(ret)));
  }

  // Maps URLs to OfflinePageItem. Items with both |urls| and |original_url|
  // will be inserted at both locations in the multimap.
  std::multimap<std::string, OfflinePageItem> url_to_offline_page_item_;
};

class TestPrefetchService : public StubPrefetchService {
 public:
  MOCK_METHOD1(SetSuggestionProvider, void(SuggestionsProvider*));
  MOCK_METHOD0(NewSuggestionsAvailable, void());
  MOCK_METHOD1(RemoveSuggestion, void(GURL));
};

void IgnoreStatus(std::vector<std::string> result) {}

void CopyStatus(std::vector<std::string>* out,
                std::vector<std::string> result) {
  *out = std::move(result);
}

void CopySuggestions(std::vector<PrefetchSuggestion>* out,
                     std::vector<PrefetchSuggestion> result) {
  *out = std::move(result);
}

}  // namespace

class FeedOfflineHostTest : public ::testing::Test {
 public:
  TestOfflinePageModel* offline_page_model() { return &offline_page_model_; }
  FeedOfflineHost* host() { return host_.get(); }
  TestPrefetchService* prefetch_service() { return &prefetch_service_; }
  int get_suggestion_consumed_count() { return suggestion_consumed_count_; }
  int get_suggestions_shown_count() { return suggestions_shown_count_; }
  int get_get_known_content_count() { return get_known_content_count_; }
  const std::vector<std::pair<std::string, bool>>& get_status_notifications() {
    return status_notifications_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SetupHost() {
    EXPECT_CALL(*offline_page_model(), AddObserver(testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*offline_page_model(), RemoveObserver(testing::_))
        .Times(1)
        .RetiresOnSaturation();
    ResetHost();
    InitHost();
  }

  void ResetHost() {
    host_ = std::make_unique<FeedOfflineHost>(
        offline_page_model(), prefetch_service(),
        base::BindRepeating(&FeedOfflineHostTest::OnSuggestionConsumed,
                            base::Unretained(this)),
        base::BindRepeating(&FeedOfflineHostTest::OnSuggestionsShown,
                            base::Unretained(this)));
  }

  void InitHost() {
    host()->Initialize(
        base::BindRepeating(&FeedOfflineHostTest::OnGetKnownContentRequested,
                            base::Unretained(this)),
        base::BindRepeating(&FeedOfflineHostTest::NotifyStatusChange,
                            base::Unretained(this)));
  }

 protected:
  FeedOfflineHostTest() { SetupHost(); }

 private:
  void OnSuggestionConsumed() { ++suggestion_consumed_count_; }
  void OnSuggestionsShown() { ++suggestions_shown_count_; }
  void OnGetKnownContentRequested() { ++get_known_content_count_; }

  void NotifyStatusChange(const std::string& url, bool available_offline) {
    status_notifications_.emplace_back(url, available_offline);
  }

  base::test::TaskEnvironment task_environment_;
  TestOfflinePageModel offline_page_model_;
  TestPrefetchService prefetch_service_;
  std::unique_ptr<FeedOfflineHost> host_;
  int suggestion_consumed_count_ = 0;
  int suggestions_shown_count_ = 0;
  int get_known_content_count_ = 0;
  std::vector<std::pair<std::string, bool>> status_notifications_;
};

TEST_F(FeedOfflineHostTest, ReportArticleListViewed) {
  EXPECT_EQ(0, get_suggestion_consumed_count());
  host()->ReportArticleListViewed();
  EXPECT_EQ(1, get_suggestion_consumed_count());
  host()->ReportArticleListViewed();
  EXPECT_EQ(2, get_suggestion_consumed_count());
  EXPECT_EQ(0, get_suggestions_shown_count());
}

TEST_F(FeedOfflineHostTest, OnSuggestionsShown) {
  EXPECT_EQ(0, get_suggestions_shown_count());
  host()->ReportArticleViewed(GURL(kUrl1));
  EXPECT_EQ(1, get_suggestions_shown_count());
  host()->ReportArticleViewed(GURL(kUrl1));
  EXPECT_EQ(2, get_suggestions_shown_count());
  EXPECT_EQ(0, get_suggestion_consumed_count());
}

TEST_F(FeedOfflineHostTest, GetOfflineStatusEmpty) {
  std::vector<std::string> actual;
  host()->GetOfflineStatus({}, base::BindOnce(&CopyStatus, &actual));
  RunUntilIdle();

  EXPECT_EQ(0U, actual.size());
}

TEST_F(FeedOfflineHostTest, GetOfflineStatusMiss) {
  offline_page_model()->AddOfflinedPage(kUrl1, 4);

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl2}, base::BindOnce(&CopyStatus, &actual));
  RunUntilIdle();

  EXPECT_EQ(0U, actual.size());
  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
  EXPECT_FALSE(host()->GetOfflineId(kUrl2).has_value());
}

TEST_F(FeedOfflineHostTest, GetOfflineStatusHit) {
  offline_page_model()->AddOfflinedPage(kUrl1, 4);
  offline_page_model()->AddOfflinedPage(kUrl2, 5);
  offline_page_model()->AddOfflinedPage(kUrl3, 6);

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl1, kUrl2},
                           base::BindOnce(&CopyStatus, &actual));

  EXPECT_EQ(0U, actual.size());
  RunUntilIdle();

  EXPECT_EQ(2U, actual.size());
  EXPECT_TRUE(actual[0] == kUrl1 || actual[1] == kUrl1);
  EXPECT_TRUE(actual[0] == kUrl2 || actual[1] == kUrl2);
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 4);
  EXPECT_EQ(host()->GetOfflineId(kUrl2).value(), 5);
  EXPECT_FALSE(host()->GetOfflineId(kUrl3).has_value());
}

TEST_F(FeedOfflineHostTest, GetOfflineIdOriginalUrl) {
  offline_page_model()->AddOfflinedPage(kUrl1, kUrl2, 4, base::Time(), "");

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl2}, base::BindOnce(&CopyStatus, &actual));
  RunUntilIdle();

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl2, actual[0]);
  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
  EXPECT_EQ(host()->GetOfflineId(kUrl2).value(), 4);
}

TEST_F(FeedOfflineHostTest, GetOfflineIdRequestUrl) {
  offline_page_model()->AddOfflinedPage(kUrl2, kUrl1, 4, base::Time(), "");

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl2}, base::BindOnce(&CopyStatus, &actual));
  RunUntilIdle();

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl2, actual[0]);
  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
  EXPECT_EQ(host()->GetOfflineId(kUrl2).value(), 4);
}

TEST_F(FeedOfflineHostTest, GetOfflineIdNewer) {
  offline_page_model()->AddOfflinedPage(kUrl1, "", 4, base::Time(), "");
  offline_page_model()->AddOfflinedPage(
      kUrl1, "", 5, base::Time() + base::TimeDelta::FromHours(1), "");

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl1}, base::BindOnce(&CopyStatus, &actual));
  RunUntilIdle();

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl1, actual[0]);
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 5);
}

TEST_F(FeedOfflineHostTest, GetOfflineIdNamespace) {
  // Even though id of 5 is newer, id of 4 will be chosen because it has the
  // preferred namespace.
  offline_page_model()->AddOfflinedPage(
      kUrl1, "", 4, base::Time(), offline_pages::kSuggestedArticlesNamespace);
  offline_page_model()->AddOfflinedPage(
      kUrl1, "", 5, base::Time() + base::TimeDelta::FromHours(1), "");

  std::vector<std::string> actual;
  host()->GetOfflineStatus({kUrl1}, base::BindOnce(&CopyStatus, &actual));
  RunUntilIdle();

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl1, actual[0]);
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 4);
}

TEST_F(FeedOfflineHostTest, GetCurrentArticleSuggestions) {
  std::vector<PrefetchSuggestion> actual;
  host()->GetCurrentArticleSuggestions(
      base::BindOnce(&CopySuggestions, &actual));
  EXPECT_EQ(1, get_get_known_content_count());
  EXPECT_EQ(0U, actual.size());

  ContentMetadata metadata;
  metadata.url = kUrl1;
  metadata.title = kOne;
  metadata.time_published = base::Time();
  metadata.image_url = kUrl2;
  metadata.publisher = kTwo;
  metadata.favicon_url = kUrl3;
  metadata.snippet = kThree;
  host()->OnGetKnownContentDone({std::move(metadata)});

  EXPECT_EQ(1U, actual.size());
  EXPECT_EQ(kUrl1, actual[0].article_url.spec());
  EXPECT_EQ(kOne, actual[0].article_title);
  EXPECT_EQ(kTwo, actual[0].article_attribution);
  EXPECT_EQ(kThree, actual[0].article_snippet);
  EXPECT_EQ(kUrl2, actual[0].thumbnail_url.spec());
  EXPECT_EQ(kUrl3, actual[0].favicon_url.spec());
}

TEST_F(FeedOfflineHostTest, GetCurrentArticleSuggestionsMultiple) {
  std::vector<PrefetchSuggestion> suggestions1;
  host()->GetCurrentArticleSuggestions(
      base::BindOnce(&CopySuggestions, &suggestions1));
  EXPECT_EQ(1, get_get_known_content_count());
  std::vector<PrefetchSuggestion> suggestions2;
  host()->GetCurrentArticleSuggestions(
      base::BindOnce(&CopySuggestions, &suggestions2));
  // This second GetCurrentArticleSuggestions() should not re-trigger since the
  // host should know there's an outstanding request.
  EXPECT_EQ(1, get_get_known_content_count());

  // Use both url and title, url goes through a GURL and isn't really moved all
  // the way, but title should actually be moved into one of the results.
  ContentMetadata metadata1;
  metadata1.url = kUrl1;
  metadata1.title = kOne;
  ContentMetadata metadata2;
  metadata2.url = kUrl2;
  metadata2.title = kTwo;
  host()->OnGetKnownContentDone({std::move(metadata1), std::move(metadata2)});

  EXPECT_EQ(2U, suggestions1.size());
  EXPECT_EQ(kUrl1, suggestions1[0].article_url.spec());
  EXPECT_EQ(kOne, suggestions1[0].article_title);
  EXPECT_EQ(kUrl2, suggestions1[1].article_url.spec());
  EXPECT_EQ(kTwo, suggestions1[1].article_title);
  EXPECT_EQ(2U, suggestions2.size());
  EXPECT_EQ(kUrl1, suggestions2[0].article_url.spec());
  EXPECT_EQ(kOne, suggestions2[0].article_title);
  EXPECT_EQ(kUrl2, suggestions2[1].article_url.spec());
  EXPECT_EQ(kTwo, suggestions2[1].article_title);

  // Now perform another GetCurrentArticleSuggestions and make sure the
  // originally bound callbacks are not invoked.
  std::vector<PrefetchSuggestion> suggestions3;
  host()->GetCurrentArticleSuggestions(
      base::BindOnce(&CopySuggestions, &suggestions3));
  EXPECT_EQ(2, get_get_known_content_count());

  ContentMetadata metadata3;
  metadata3.url = kUrl3;
  metadata3.title = kThree;
  host()->OnGetKnownContentDone({std::move(metadata3)});

  EXPECT_EQ(2U, suggestions1.size());
  EXPECT_EQ(2U, suggestions2.size());
  EXPECT_EQ(1U, suggestions3.size());
  EXPECT_EQ(kUrl3, suggestions3[0].article_url.spec());
  EXPECT_EQ(kThree, suggestions3[0].article_title);
}

TEST_F(FeedOfflineHostTest, OfflinePageAdded) {
  OfflinePageItem added_page;
  added_page.url = GURL(kUrl1);
  added_page.original_url_if_different = GURL(kUrl2);
  added_page.offline_id = 4;

  host()->OfflinePageAdded(nullptr, added_page);

  EXPECT_EQ(1U, get_status_notifications().size());
  EXPECT_EQ(kUrl2, get_status_notifications()[0].first);
  EXPECT_TRUE(get_status_notifications()[0].second);
  EXPECT_EQ(host()->GetOfflineId(kUrl2).value(), 4);
}

TEST_F(FeedOfflineHostTest, OfflinePageDeleted) {
  offline_page_model()->AddOfflinedPage(kUrl1, 4);
  host()->GetOfflineStatus({kUrl1}, base::BindOnce(&IgnoreStatus));
  RunUntilIdle();
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 4);
  OfflinePageItem page_item;
  page_item.url = GURL(kUrl1);

  host()->OfflinePageDeleted(page_item);

  EXPECT_EQ(1U, get_status_notifications().size());
  EXPECT_EQ(kUrl1, get_status_notifications()[0].first);
  EXPECT_FALSE(get_status_notifications()[0].second);
  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
}

TEST_F(FeedOfflineHostTest, OnNoListeners) {
  offline_page_model()->AddOfflinedPage(kUrl1, 4);
  host()->GetOfflineStatus({kUrl1}, base::BindOnce(&IgnoreStatus));
  RunUntilIdle();
  EXPECT_EQ(host()->GetOfflineId(kUrl1).value(), 4);

  host()->OnNoListeners();

  EXPECT_FALSE(host()->GetOfflineId(kUrl1).has_value());
}

TEST_F(FeedOfflineHostTest, NoAddObserverWithoutInitialize) {
  // Normally SetupHost() will set all observer expectations. Here we explicitly
  // do not expect AddObserver and then reset the host.
  EXPECT_CALL(*offline_page_model(), RemoveObserver(testing::_))
      .Times(1)
      .RetiresOnSaturation();
  ResetHost();
}

TEST_F(FeedOfflineHostTest, SetSuggestionProviderNotCalledSynchronously) {
  EXPECT_CALL(*prefetch_service(), SetSuggestionProvider(testing::_)).Times(0);
}

TEST_F(FeedOfflineHostTest, SetSuggestionProviderCalledAsync) {
  EXPECT_CALL(*prefetch_service(), SetSuggestionProvider(testing::_)).Times(1);
  RunUntilIdle();
}

TEST_F(FeedOfflineHostTest, OnNewContentReceived) {
  EXPECT_CALL(*prefetch_service(), NewSuggestionsAvailable()).Times(1);
  host()->OnNewContentReceived();
}

TEST_F(FeedOfflineHostTest, RemoveZeroSuggestions) {
  EXPECT_CALL(*prefetch_service(), RemoveSuggestion(testing::_)).Times(0);
  host()->OnContentRemoved({});
}

TEST_F(FeedOfflineHostTest, RemoveOneSuggestion) {
  EXPECT_CALL(*prefetch_service(), RemoveSuggestion(EqualsSpec(kUrl1)))
      .Times(1);
  host()->OnContentRemoved({kUrl1});
}

TEST_F(FeedOfflineHostTest, RemoveTwoSuggestions) {
  EXPECT_CALL(*prefetch_service(), RemoveSuggestion(EqualsSpec(kUrl1)))
      .Times(1);
  EXPECT_CALL(*prefetch_service(), RemoveSuggestion(EqualsSpec(kUrl2)))
      .Times(1);
  host()->OnContentRemoved({kUrl1, kUrl2});
}

}  // namespace feed
