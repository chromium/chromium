// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/prefetched_pages_tracker_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ntp_snippets::test::FakeOfflinePageModel;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModel;
using offline_pages::PageCriteria;
using testing::_;
using testing::Eq;
using testing::SaveArg;
using testing::StrictMock;

namespace ntp_snippets {

namespace {

const int64_t kSystemDownloadId = 0;

class MockOfflinePageModel : public offline_pages::StubOfflinePageModel {
 public:
  ~MockOfflinePageModel() override = default;

  MOCK_METHOD2(GetPagesWithCriteria,
               void(const PageCriteria& criteria,
                    MultipleOfflinePageItemCallback callback));
};

OfflinePageItem CreateOfflinePageItem(const GURL& url,
                                      const std::string& name_space) {
  static int id = 0;
  ++id;
  return OfflinePageItem(
      url, id, offline_pages::ClientId(name_space, base::NumberToString(id)),
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("some/folder/%d.mhtml", id)),
      0, base::Time::Now());
}

}  // namespace

class PrefetchedPagesTrackerImplTest : public ::testing::Test {
 public:
  PrefetchedPagesTrackerImplTest() = default;

  FakeOfflinePageModel* fake_offline_page_model() {
    return &fake_offline_page_model_;
  }

  MockOfflinePageModel* mock_offline_page_model() {
    return &mock_offline_page_model_;
  }

 private:
  FakeOfflinePageModel fake_offline_page_model_;
  StrictMock<MockOfflinePageModel> mock_offline_page_model_;
  DISALLOW_COPY_AND_ASSIGN(PrefetchedPagesTrackerImplTest);
};

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldRetrievePrefetchedEarlierSuggestionsOnInitialize) {
  (*fake_offline_page_model()->mutable_items()) = {
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace)};
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_FALSE(
      tracker.PrefetchedOfflinePageExists(GURL("http://not_added_url.com")));
  EXPECT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldAddNewPrefetchedPagesWhenNotified) {
  fake_offline_page_model()->mutable_items()->clear();
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_FALSE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
  tracker.OfflinePageAdded(
      fake_offline_page_model(),
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace));
  EXPECT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldIgnoreOtherTypesOfOfflinePagesWhenNotified) {
  fake_offline_page_model()->mutable_items()->clear();
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_FALSE(tracker.PrefetchedOfflinePageExists(
      GURL("http://manually_downloaded.com")));
  tracker.OfflinePageAdded(
      fake_offline_page_model(),
      CreateOfflinePageItem(GURL("http://manually_downloaded.com"),
                            offline_pages::kNTPSuggestionsNamespace));
  EXPECT_FALSE(tracker.PrefetchedOfflinePageExists(
      GURL("http://manually_downloaded.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldIgnoreOtherTypesOfOfflinePagesOnStartup) {
  (*fake_offline_page_model()->mutable_items()) = {
      CreateOfflinePageItem(GURL("http://manually_downloaded.com"),
                            offline_pages::kNTPSuggestionsNamespace)};
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_FALSE(tracker.PrefetchedOfflinePageExists(
      GURL("http://manually_downloaded.com")));
  EXPECT_FALSE(tracker.PrefetchedOfflinePageExists(
      GURL("http://manually_downloaded.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest, ShouldDeletePrefetchedURLWhenNotified) {
  const OfflinePageItem item =
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace);
  (*fake_offline_page_model()->mutable_items()) = {item};
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
  OfflinePageItem deleted_item = item;
  deleted_item.system_download_id = kSystemDownloadId;
  tracker.OfflinePageDeleted(deleted_item);
  EXPECT_FALSE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldIgnoreDeletionOfOtherTypeOfflinePagesWhenNotified) {
  const OfflinePageItem prefetched_item =
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace);
  // The URL is intentionally the same.
  const OfflinePageItem manually_downloaded_item = CreateOfflinePageItem(
      GURL("http://prefetched.com"), offline_pages::kNTPSuggestionsNamespace);
  (*fake_offline_page_model()->mutable_items()) = {prefetched_item,
                                                   manually_downloaded_item};
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
  OfflinePageItem deleted_item = manually_downloaded_item;
  deleted_item.system_download_id = kSystemDownloadId;
  tracker.OfflinePageDeleted(deleted_item);
  EXPECT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldReportAsNotInitializedBeforeReceivedArticles) {
  EXPECT_CALL(*mock_offline_page_model(), GetPagesWithCriteria(_, _));
  PrefetchedPagesTrackerImpl tracker(mock_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));
  EXPECT_FALSE(tracker.IsInitialized());
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldReportAsInitializedAfterInitialization) {
  MultipleOfflinePageItemCallback offline_pages_callback;
  EXPECT_CALL(*mock_offline_page_model(), GetPagesWithCriteria(_, _))
      .WillOnce(
          [&](const PageCriteria&, MultipleOfflinePageItemCallback callback) {
            offline_pages_callback = std::move(callback);
          });
  PrefetchedPagesTrackerImpl tracker(mock_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_FALSE(tracker.IsInitialized());
  std::move(offline_pages_callback).Run(std::vector<OfflinePageItem>());
  EXPECT_TRUE(tracker.IsInitialized());
}

TEST_F(PrefetchedPagesTrackerImplTest, ShouldCallCallbackAfterInitialization) {
  MultipleOfflinePageItemCallback offline_pages_callback;
  EXPECT_CALL(*mock_offline_page_model(), GetPagesWithCriteria(_, _))
      .WillOnce(
          [&](const PageCriteria&, MultipleOfflinePageItemCallback callback) {
            offline_pages_callback = std::move(callback);
          });
  PrefetchedPagesTrackerImpl tracker(mock_offline_page_model());

  base::MockCallback<base::OnceCallback<void()>>
      mock_initialization_completed_callback;
  tracker.Initialize(mock_initialization_completed_callback.Get());
  EXPECT_CALL(mock_initialization_completed_callback, Run());
  std::move(offline_pages_callback).Run(std::vector<OfflinePageItem>());
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldCallMultipleCallbacksAfterInitialization) {
  MultipleOfflinePageItemCallback offline_pages_callback;
  EXPECT_CALL(*mock_offline_page_model(), GetPagesWithCriteria(_, _))
      .WillOnce(
          [&](const PageCriteria&, MultipleOfflinePageItemCallback callback) {
            offline_pages_callback = std::move(callback);
          });
  PrefetchedPagesTrackerImpl tracker(mock_offline_page_model());

  base::MockCallback<base::OnceCallback<void()>>
      first_mock_initialization_completed_callback,
      second_mock_initialization_completed_callback;
  tracker.Initialize(first_mock_initialization_completed_callback.Get());
  tracker.Initialize(second_mock_initialization_completed_callback.Get());
  EXPECT_CALL(first_mock_initialization_completed_callback, Run());
  EXPECT_CALL(second_mock_initialization_completed_callback, Run());
  std::move(offline_pages_callback).Run(std::vector<OfflinePageItem>());
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldCallCallbackImmediatelyIfAlreadyInitialiazed) {
  MultipleOfflinePageItemCallback offline_pages_callback;
  EXPECT_CALL(*mock_offline_page_model(), GetPagesWithCriteria(_, _))
      .WillOnce(
          [&](const PageCriteria&, MultipleOfflinePageItemCallback callback) {
            offline_pages_callback = std::move(callback);
          });
  PrefetchedPagesTrackerImpl tracker(mock_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  std::move(offline_pages_callback).Run(std::vector<OfflinePageItem>());

  base::MockCallback<base::OnceCallback<void()>>
      mock_initialization_completed_callback;
  EXPECT_CALL(mock_initialization_completed_callback, Run());
  tracker.Initialize(mock_initialization_completed_callback.Get());
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldKeepPrefetchedURLAfterDuplicatePageDeleted) {
  const OfflinePageItem first_item =
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace);
  const OfflinePageItem second_item =
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace);
  (*fake_offline_page_model()->mutable_items()) = {first_item, second_item};
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));

  OfflinePageItem deleted_item = first_item;
  deleted_item.system_download_id = kSystemDownloadId;
  tracker.OfflinePageDeleted(deleted_item);

  // Only one offline page (out of two) has been removed, the remaining one
  // should be reported here.
  EXPECT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
}

TEST_F(PrefetchedPagesTrackerImplTest,
       ShouldDeletePrefetchedURLAfterAllItsPagesAreDeleted) {
  const OfflinePageItem first_item =
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace);
  const OfflinePageItem second_item =
      CreateOfflinePageItem(GURL("http://prefetched.com"),
                            offline_pages::kSuggestedArticlesNamespace);
  (*fake_offline_page_model()->mutable_items()) = {first_item, second_item};
  PrefetchedPagesTrackerImpl tracker(fake_offline_page_model());
  tracker.Initialize(base::BindOnce([] {}));

  ASSERT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));

  OfflinePageItem first_deleted_item = first_item;
  first_deleted_item.system_download_id = kSystemDownloadId;
  tracker.OfflinePageDeleted(first_deleted_item);

  ASSERT_TRUE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));

  OfflinePageItem second_deleted_item = second_item;
  second_deleted_item.system_download_id = kSystemDownloadId;
  tracker.OfflinePageDeleted(second_deleted_item);

  // All offline pages have been removed, their absence should be reported here.
  EXPECT_FALSE(
      tracker.PrefetchedOfflinePageExists(GURL("http://prefetched.com")));
}

}  // namespace ntp_snippets
