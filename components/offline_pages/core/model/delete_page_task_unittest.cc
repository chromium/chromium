// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/delete_page_task.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

using DeletedPageInfo = OfflinePageModel::DeletedPageInfo;

namespace {

const char kTestNamespace[] = "default";
const GURL kTestUrl1("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const int64_t kTestOfflineIdNoMatch = 20170905LL;
const ClientId kTestClientIdNoMatch(kTestNamespace, "20170905");

}  // namespace

class DeletePageTaskTest : public ModelTaskTestBase {
 public:
  DeletePageTaskTest();
  ~DeletePageTaskTest() override;

  void SetUp() override;

  void ResetResults();

  void OnDeletePageDone(DeletePageResult result,
                        const std::vector<DeletedPageInfo>& deleted_page_infos);
  bool CheckPageDeleted(const OfflinePageItem& page);
  DeletePageTask::DeletePageTaskCallback delete_page_callback();

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  const base::Optional<DeletePageResult>& last_delete_page_result() {
    return last_delete_page_result_;
  }
  const std::vector<DeletedPageInfo>& last_deleted_page_infos() {
    return last_deleted_page_infos_;
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  base::Optional<DeletePageResult> last_delete_page_result_;
  std::vector<DeletedPageInfo> last_deleted_page_infos_;
};

DeletePageTaskTest::DeletePageTaskTest() {}

DeletePageTaskTest::~DeletePageTaskTest() {}

void DeletePageTaskTest::SetUp() {
  ModelTaskTestBase::SetUp();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void DeletePageTaskTest::OnDeletePageDone(
    DeletePageResult result,
    const std::vector<DeletedPageInfo>& deleted_page_infos) {
  last_delete_page_result_ = result;
  last_deleted_page_infos_ = deleted_page_infos;
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

TEST_F(DeletePageTaskTest, DeletePageByOfflineId) {
  // Add 3 pages and try to delete 2 of them using offline id.
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  // Set an access count of 200 to avoid falling in the same bucket.
  generator()->SetAccessCount(200);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // The pages with the offline ids will be removed from the store.
  std::vector<int64_t> offline_ids({page1.offline_id, page3.offline_id});
  auto task = DeletePageTask::CreateTaskMatchingOfflineIds(
      store(), delete_page_callback(), offline_ids);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_TRUE(CheckPageDeleted(page3));
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

TEST_F(DeletePageTaskTest, DeletePageByOfflineIdNotFound) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());

  // The pages with the offline ids will be removed from the store. But since
  // the id isn't in the store, there will be no pages deleted and the result
  // will be success since there's no NOT_FOUND anymore.
  // This *might* break if any of the generated offline ids above equals to the
  // constant value defined above.
  std::vector<int64_t> offline_ids({kTestOfflineIdNoMatch});
  auto task = DeletePageTask::CreateTaskMatchingOfflineIds(
      store(), delete_page_callback(), offline_ids);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
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

TEST_F(DeletePageTaskTest, DeletePageByClientId) {
  // Add 3 pages and try to delete 2 of them using client id.
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  generator()->SetAccessCount(200);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  std::vector<ClientId> client_ids({page1.client_id, page3.client_id});
  auto task = DeletePageTask::CreateTaskMatchingClientIds(
      store(), delete_page_callback(), client_ids);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_TRUE(CheckPageDeleted(page3));
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

TEST_F(DeletePageTaskTest, DeletePageByClientIdNotFound) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());

  // The pages with the client ids will be removed from the store. But since
  // the id isn't in the store, there will be no pages deleted and the result
  // will be success since there's no NOT_FOUND anymore.
  std::vector<ClientId> client_ids({kTestClientIdNoMatch});
  auto task = DeletePageTask::CreateTaskMatchingClientIds(
      store(), delete_page_callback(), client_ids);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      0);
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      0);
}

TEST_F(DeletePageTaskTest, DeletePageByClientIdAndOrigin) {
  // Add 3 pages and try to delete 2 of them using client id and origin
  // Page1: {test namespace, random id, abc.xyz}
  // Page2: {test namespace, foo, abc.xyz}
  // Page3: {test namespace, foo, <none>}
  generator()->SetNamespace(kTestNamespace);
  generator()->SetRequestOrigin("abc.xyz");
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  generator()->SetId("foo");
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  generator()->SetRequestOrigin("");
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  std::vector<ClientId> client_ids({page1.client_id, page2.client_id});
  auto task = DeletePageTask::CreateTaskMatchingClientIdsAndOrigin(
      store(), delete_page_callback(), client_ids, "abc.xyz");
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageDeleted(page1));
  EXPECT_TRUE(CheckPageDeleted(page2));
  EXPECT_FALSE(CheckPageDeleted(page3));
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      2);
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      2);
}

TEST_F(DeletePageTaskTest, DeletePageByClientIdAndOriginNotFound) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());

  // The pages with the client ids will be removed from the store given that
  // their origin matches. But since the origin isn't in the store, there will
  // be no pages deleted and the result will be success since there's no
  // NOT_FOUND anymore.
  std::vector<ClientId> client_ids(
      {page1.client_id, page2.client_id, page3.client_id});
  auto task = DeletePageTask::CreateTaskMatchingClientIdsAndOrigin(
      store(), delete_page_callback(), client_ids, "abc.xyz");
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.PageLifetime"),
      0);
  histogram_tester()->ExpectTotalCount(
      model_utils::AddHistogramSuffix(page1.client_id.name_space,
                                      "OfflinePages.AccessCount"),
      0);
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
      store(), delete_page_callback(), policy_controller(), predicate);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
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
      store(), delete_page_callback(), policy_controller(), predicate);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
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
  base::Time now = base::Time::Now();
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
      store(), delete_page_callback(), policy_controller(), page);
  RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(1UL, last_deleted_page_infos().size());
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
      store(), delete_page_callback(), policy_controller(), page);
  RunTask(std::move(task));

  // Since there's no limit for page per url of Download Namespace, the result
  // should be success with no page deleted.
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
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
