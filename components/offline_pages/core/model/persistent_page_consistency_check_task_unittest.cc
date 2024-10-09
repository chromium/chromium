// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/persistent_page_consistency_check_task.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::A;
using testing::Eq;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace offline_pages {

using PersistentPageConsistencyCheckCallback =
    PersistentPageConsistencyCheckTask::PersistentPageConsistencyCheckCallback;

class PersistentPageConsistencyCheckTaskTest : public ModelTaskTestBase {
 public:
  PersistentPageConsistencyCheckTaskTest() = default;
  ~PersistentPageConsistencyCheckTaskTest() override = default;

  void SetUp() override {
    ModelTaskTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  bool IsPageMissingFile(const OfflinePageItem& page) {
    auto actual_page = store_test_util()->GetPageByOfflineId(page.offline_id);
    return (actual_page && actual_page->file_missing_time != base::Time());
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ClearExpiredPersistentPages DISABLED_ClearExpiredPersistentPages
#else
#define MAYBE_ClearExpiredPersistentPages ClearExpiredPersistentPages
#endif
TEST_F(PersistentPageConsistencyCheckTaskTest,
       MAYBE_ClearExpiredPersistentPages) {
  base::Time expire_time = base::Time::Now() - base::Days(400);

  // |page{1,4}| will be marked as missing file.
  // |page{2,5}| will be deleted from DB, since they've been expired for longer
  // than threshold.
  // |page{3,6}| will remove the file_missing_time from the entry, since
  // they've been missing files but the files appeared again.
  generator()->SetUseOfflineIdAsSystemDownloadId(true);
  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem page1 = AddPageWithoutFile();
  generator()->SetFileMissingTime(expire_time);
  OfflinePageItem page2 = AddPageWithoutFile();
  OfflinePageItem page3 = AddPage();

  generator()->SetArchiveDirectory(PublicDir());
  generator()->SetFileMissingTime(base::Time());
  OfflinePageItem page4 = AddPageWithoutFile();
  generator()->SetFileMissingTime(expire_time);
  OfflinePageItem page5 = AddPageWithoutFile();
  OfflinePageItem page6 = AddPage();

  EXPECT_EQ(6LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PublicDir()));

  base::MockCallback<PersistentPageConsistencyCheckCallback> callback;
  EXPECT_CALL(
      callback,
      Run(Eq(true),
          UnorderedElementsAre(
              PublishedArchiveId{page2.system_download_id, page2.file_path},
              PublishedArchiveId{page5.system_download_id, page5.file_path})));

  RunTask(std::make_unique<PersistentPageConsistencyCheckTask>(
      store(), archive_manager(), base::Time::Now(), callback.Get()));

  EXPECT_EQ(4LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PublicDir()));
  EXPECT_TRUE(store_test_util()->GetPageByOfflineId(page1.offline_id));
  EXPECT_TRUE(IsPageMissingFile(page1));
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page2.offline_id));
  EXPECT_TRUE(store_test_util()->GetPageByOfflineId(page3.offline_id));
  EXPECT_FALSE(IsPageMissingFile(page3));
  EXPECT_TRUE(store_test_util()->GetPageByOfflineId(page4.offline_id));
  EXPECT_TRUE(IsPageMissingFile(page4));
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page5.offline_id));
  EXPECT_TRUE(store_test_util()->GetPageByOfflineId(page6.offline_id));
  EXPECT_FALSE(IsPageMissingFile(page6));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_ClearExpiredPersistentPagesByFilePath \
  DISABLED_ClearExpiredPersistentPagesByFilePath
#else
#define MAYBE_ClearExpiredPersistentPagesByFilePath \
  ClearExpiredPersistentPagesByFilePath
#endif
TEST_F(PersistentPageConsistencyCheckTaskTest,
       MAYBE_ClearExpiredPersistentPagesByFilePath) {
  base::Time expire_time = base::Time::Now() - base::Days(400);
  // |page| will be deleted from DB, since it's been expired for longer than
  // threshold.
  generator()->SetSystemDownloadId(kArchivePublishedWithoutDownloadId);
  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PublicDir());
  generator()->SetFileMissingTime(expire_time);
  OfflinePageItem page = AddPageWithoutFile();

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());

  base::MockCallback<PersistentPageConsistencyCheckCallback> callback;
  EXPECT_CALL(callback,
              Run(Eq(true), UnorderedElementsAre(PublishedArchiveId{
                                page.system_download_id, page.file_path})));

  RunTask(std::make_unique<PersistentPageConsistencyCheckTask>(
      store(), archive_manager(), base::Time::Now(), callback.Get()));

  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page.offline_id));
}

}  // namespace offline_pages
