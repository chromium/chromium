// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/delete_page_task.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

const char kTestNamespace[] = "default";
const GURL kTestUrl1("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const ClientId kTestClientIdNoMatch(kTestNamespace, "20170905");

GURL OriginalUrl() {
  return GURL("http://original.com");
}

}  // namespace

class DeletePageTaskTest : public ModelTaskTestBase {
 public:
  DeletePageTaskTest();
  ~DeletePageTaskTest() override;

  void SetUp() override;

  void ResetResults();

  void OnDeletePageDone(DeletePageResult result,
                        const std::vector<OfflinePageItem>& deleted_pages);
  bool CheckPageDeleted(const OfflinePageItem& page);
  DeletePageTask::DeletePageTaskCallback delete_page_callback();

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  const base::Optional<DeletePageResult>& last_delete_page_result() {
    return last_delete_page_result_;
  }
  const std::vector<OfflinePageItem>& last_deleted_page_items() {
    return last_deleted_page_items_;
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  base::Optional<DeletePageResult> last_delete_page_result_;
  std::vector<OfflinePageItem> last_deleted_page_items_;
};

DeletePageTaskTest::DeletePageTaskTest() {}

DeletePageTaskTest::~DeletePageTaskTest() {}

void DeletePageTaskTest::SetUp() {
  ModelTaskTestBase::SetUp();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void DeletePageTaskTest::OnDeletePageDone(
    DeletePageResult result,
    const std::vector<OfflinePageItem>& deleted_page_items) {
  last_delete_page_result_ = result;
  last_deleted_page_items_ = deleted_page_items;
}

DeletePageTask::DeletePageTaskCallback
DeletePageTaskTest::delete_page_callback() {
  return base::BindOnce(&DeletePageTaskTest::OnDeletePageDone,
                        base::AsWeakPtr(this));
}

bool DeletePageTaskTest::CheckPageDeleted(const OfflinePageItem& page) {
  auto stored_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  return !base::PathExists(page.file_path) && !stored_page;
}

// Delete a page and verify all the information in deleted_pages is accurate.
TEST_F(DeletePageTaskTest, OfflinePageItemIsPopulated) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  page1.url = kTestUrl1;
  page1.original_url_if_different = OriginalUrl();
  page1.request_origin = "test-origin";
  page1.system_download_id = 1234;
  store_test_util()->InsertItem(page1);

  // Run DeletePageTask for to delete the page.
  PageCriteria criteria;
  criteria.offline_ids = std::vector<int64_t>{page1.offline_id};
  auto task = DeletePageTask::CreateTaskWithCriteria(store(), criteria,
                                                     delete_page_callback());
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(1UL, last_deleted_page_items().size());

  // Verify original_url is returned via OfflinePageItem.
  const OfflinePageItem& item = last_deleted_page_items()[0];
  EXPECT_EQ(page1.url, item.url);
  EXPECT_EQ(page1.client_id, item.client_id);
  EXPECT_EQ(page1.request_origin, item.request_origin);
  EXPECT_EQ(page1.system_download_id, item.system_download_id);
  EXPECT_EQ(page1.offline_id, item.offline_id);
  EXPECT_EQ(OriginalUrl(), item.original_url_if_different);
  EXPECT_EQ(OriginalUrl(), item.GetOriginalUrl());
}

TEST_F(DeletePageTaskTest, DeletePageByUrlPredicate) {
  // Add 3 pages and try to delete 2 of them using url predicate.
  generator()->SetNamespace(kTestNamespace);
  generator()->SetUrl(kTestUrl1);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  generator()->SetAccessCount(200);
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  generator()->SetUrl(kTestUrl2);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // Delete all pages with url contains example.com, which are with kTestUrl1.
  UrlPredicate predicate = base::BindRepeating([](const GURL& url) -> bool {
    return url.spec().find("example.com") != std::string::npos;
  });

  auto task = DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
      store(), delete_page_callback(), predicate);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_items().size());
  EXPECT_EQ(predicate.Run(page1.url), CheckPageDeleted(page1));
  EXPECT_EQ(predicate.Run(page2.url), CheckPageDeleted(page2));
  EXPECT_EQ(predicate.Run(page3.url), CheckPageDeleted(page3));
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      2);
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      2);
  histogram_tester()->ExpectBucketCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      0, 1);
  histogram_tester()->ExpectBucketCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      200, 1);
}

TEST_F(DeletePageTaskTest, DeletePageByUrlPredicateNotFound) {
  // Add 3 pages and try to delete 2 of them using url predicate.
  generator()->SetNamespace(kTestNamespace);
  generator()->SetUrl(kTestUrl1);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  generator()->SetUrl(kTestUrl2);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // Return false for all pages so that no pages will be deleted.
  UrlPredicate predicate =
      base::BindRepeating([](const GURL& url) -> bool { return false; });

  auto task = DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
      store(), delete_page_callback(), predicate);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_items().size());
  EXPECT_FALSE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_FALSE(CheckPageDeleted(page3));
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      0);
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      0);
}

TEST_F(DeletePageTaskTest, DeletePageForPageLimit) {
  // Add 3 pages, the kTestNamespace has a limit of 1 for page per url.
  generator()->SetNamespace(kTestNamespace);
  generator()->SetUrl(kTestUrl1);
  // Guarantees that page1 will be deleted by making it older.
  base::Time now = OfflineTimeNow();
  generator()->SetLastAccessTime(now - base::TimeDelta::FromMinutes(5));
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  generator()->SetLastAccessTime(now);
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page = generator()->CreateItem();
  generator()->SetUrl(kTestUrl2);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  auto task = DeletePageTask::CreateTaskDeletingForPageLimit(
      store(), delete_page_callback(), page);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(1UL, last_deleted_page_items().size());
  EXPECT_TRUE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_FALSE(CheckPageDeleted(page3));
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      1);
  histogram_tester()->ExpectUniqueSample(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      0, 1);
}

TEST_F(DeletePageTaskTest, DeletePageForPageLimit_UnlimitedNamespace) {
  // Add 3 pages, the kTestNamespace has a limit of 1 for page per url.
  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetUrl(kTestUrl1);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page = generator()->CreateItem();
  generator()->SetUrl(kTestUrl2);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  auto task = DeletePageTask::CreateTaskDeletingForPageLimit(
      store(), delete_page_callback(), page);
  RunTask(std::move(task));

  // Since there's no limit for page per url of Download Namespace, the result
  // should be success with no page deleted.
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_items().size());
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      0);
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      0);
}

}  // namespace offline_pages
