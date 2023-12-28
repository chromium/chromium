// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/mark_page_accessed_task.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const int64_t kTestOfflineId = 1234LL;
const char kTestClientNamespace[] = "default";
const ClientId kTestClientId(kTestClientNamespace, "1234");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("/test/path/file"));
const int64_t kTestFileSize = 876543LL;

}  // namespace

class MarkPageAccessedTaskTest : public ModelTaskTestBase {
 public:
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(MarkPageAccessedTaskTest, MarkPageAccessed) {
  const GURL kTestUrl("http://example.com");
  OfflinePageItem page(kTestUrl, kTestOfflineId, kTestClientId, kTestFilePath,
                       kTestFileSize);
  store_test_util()->InsertItem(page);

  base::Time current_time = OfflineTimeNow();
  auto task = std::make_unique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                     current_time);
  RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestUrl, offline_page->url);
  EXPECT_EQ(kTestClientId, offline_page->client_id);
  EXPECT_EQ(kTestFileSize, offline_page->file_size);
  EXPECT_EQ(1, offline_page->access_count);
  EXPECT_EQ(current_time, offline_page->last_access_time);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(kTestClientId.name_space)),
      1);
}

TEST_F(MarkPageAccessedTaskTest, MarkPageAccessedTwice) {
  const GURL kTestUrl("http://example.com");
  OfflinePageItem page(kTestUrl, kTestOfflineId, kTestClientId, kTestFilePath,
                       kTestFileSize);
  store_test_util()->InsertItem(page);

  base::Time current_time = OfflineTimeNow();
  auto task = std::make_unique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                     current_time);
  RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestOfflineId, offline_page->offline_id);
  EXPECT_EQ(kTestUrl, offline_page->url);
  EXPECT_EQ(kTestClientId, offline_page->client_id);
  EXPECT_EQ(kTestFileSize, offline_page->file_size);
  EXPECT_EQ(1, offline_page->access_count);
  EXPECT_EQ(current_time, offline_page->last_access_time);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(kTestClientId.name_space)),
      1);

  base::Time second_time = OfflineTimeNow();
  task = std::make_unique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                second_time);
  RunTask(std::move(task));

  offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestOfflineId, offline_page->offline_id);
  EXPECT_EQ(2, offline_page->access_count);
  EXPECT_EQ(second_time, offline_page->last_access_time);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(kTestClientId.name_space)),
      2);
}

}  // namespace offline_pages
