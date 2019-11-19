// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_storage_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using ClearStorageResult = ClearStorageTask::ClearStorageResult;
using StorageStats = ArchiveManager::StorageStats;

namespace {

const GURL kTestUrl("http://example.com");
const int64_t kTestFileSize = 1 << 19;              // Make a page 512KB.
const int64_t kFreeSpaceNormal = 1000 * (1 << 20);  // 1000MB free space.

enum TestOptions {
  DEFAULT = 1 << 0,
  DELETE_FAILURE = 1 << 1,
};

struct PageSettings {
  std::string name_space;
  int fresh_page_count;
  int expired_page_count;
};

class TestArchiveManager : public ArchiveManager {
 public:
  explicit TestArchiveManager(const base::FilePath& temp_archive_dir)
      : free_space_(kFreeSpaceNormal),
        temporary_archive_dir_(temp_archive_dir) {}

  void GetStorageStats(
      base::OnceCallback<void(const StorageStats& storage_stats)> callback)
      const override {
    StorageStats stats;
    stats.internal_free_disk_space = free_space_;
    base::FileEnumerator file_enumerator(temporary_archive_dir_, false,
                                         base::FileEnumerator::FILES);
    int temp_file_count = 0;
    while (!file_enumerator.Next().empty())
      temp_file_count++;
    stats.temporary_archives_size = temp_file_count * kTestFileSize;
    std::move(callback).Run(stats);
  }

  void SetFreeSpace(int64_t free_space) { free_space_ = free_space; }

 private:
  int64_t free_space_;
  base::FilePath temporary_archive_dir_;
};

}  // namespace

class ClearStorageTaskTest : public ModelTaskTestBase {
 public:
  ClearStorageTaskTest();
  ~ClearStorageTaskTest() override;

  void SetUp() override;

  void Initialize(const std::vector<PageSettings>& settings,
                  TestOptions options = TestOptions::DEFAULT);
  void OnClearStorageDone(size_t cleared_page_count, ClearStorageResult result);
  void AddPages(const PageSettings& setting);
  void RunClearStorageTask(const base::Time& start_time);

  void SetFreeSpace(int64_t free_space) {
    archive_manager_->SetFreeSpace(free_space);
  }

  ArchiveManager* archive_manager() { return archive_manager_.get(); }
  TestScopedOfflineClock* clock() { return &clock_; }
  size_t last_cleared_page_count() { return last_cleared_page_count_; }
  int total_cleared_times() { return total_cleared_times_; }
  ClearStorageResult last_clear_storage_result() {
    return last_clear_storage_result_;
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<TestArchiveManager> archive_manager_;
  TestScopedOfflineClock clock_;

  size_t last_cleared_page_count_;
  int total_cleared_times_;
  ClearStorageResult last_clear_storage_result_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ClearStorageTaskTest::ClearStorageTaskTest()
    : last_cleared_page_count_(0),
      total_cleared_times_(0),
      last_clear_storage_result_(ClearStorageResult::SUCCESS) {}

ClearStorageTaskTest::~ClearStorageTaskTest() {}

void ClearStorageTaskTest::SetUp() {
  ModelTaskTestBase::SetUp();
  // Setting up policies for testing.
  clock_.SetNow(base::Time::Now());
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void ClearStorageTaskTest::Initialize(
    const std::vector<PageSettings>& page_settings,
    TestOptions options) {
  generator()->SetFileSize(kTestFileSize);

  // Adding pages based on |page_settings|.
  for (const auto& setting : page_settings)
    AddPages(setting);
  archive_manager_.reset(new TestArchiveManager(TemporaryDir()));
}

void ClearStorageTaskTest::OnClearStorageDone(size_t cleared_page_count,
                                              ClearStorageResult result) {
  last_cleared_page_count_ = cleared_page_count;
  last_clear_storage_result_ = result;
  total_cleared_times_++;
}

void ClearStorageTaskTest::AddPages(const PageSettings& setting) {
  // Note: even though the creation time set below is inconsistent with the last
  // access time set further down, these values are used independently and
  // this choice allows for easier testing of the TimeSinceCreation metric. This
  // way we can work directly with the times used to advance the testing clock
  // during each test.

  // Make sure no persistent pages are marked as expired.
  const OfflinePageClientPolicy& policy = GetPolicy(setting.name_space);
  if (policy.lifetime_type == LifetimeType::PERSISTENT)
    ASSERT_FALSE(setting.expired_page_count);

  generator()->SetCreationTime(clock()->Now());
  generator()->SetNamespace(setting.name_space);
  if (policy.lifetime_type == LifetimeType::TEMPORARY) {
    generator()->SetArchiveDirectory(TemporaryDir());
  } else {
    generator()->SetArchiveDirectory(PrivateDir());
  }

  generator()->SetLastAccessTime(clock_.Now());
  for (int i = 0; i < setting.fresh_page_count; ++i) {
    AddPage();
  }

  generator()->SetLastAccessTime(clock_.Now() - policy.expiration_period);
  for (int i = 0; i < setting.expired_page_count; ++i) {
    AddPage();
  }
}

void ClearStorageTaskTest::RunClearStorageTask(const base::Time& start_time) {
  auto task = std::make_unique<ClearStorageTask>(
      store(), archive_manager(), start_time,
      base::BindOnce(&ClearStorageTaskTest::OnClearStorageDone,
                     base::AsWeakPtr(this)));

  RunTask(std::move(task));
}

TEST_F(ClearStorageTaskTest, ClearPagesLessThanLimit) {
  Initialize({{kBookmarkNamespace, 1, 1}, {kLastNNamespace, 1, 1}});

  clock()->Advance(base::TimeDelta::FromMinutes(5));
  RunClearStorageTask(clock()->Now());

  // In total there're 2 expired pages so they'll be cleared successfully.
  // There will be 2 pages remaining in the store, and make sure their files
  // weren't cleared.
  EXPECT_EQ(2UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 5, 2);
}

TEST_F(ClearStorageTaskTest, ClearPagesMoreFreshPages) {
  Initialize({{kBookmarkNamespace, 30, 0}, {kLastNNamespace, 100, 1}});

  clock()->Advance(base::TimeDelta::FromMinutes(5));
  RunClearStorageTask(clock()->Now());

  // In total there's 1 expired page so it'll be cleared successfully.
  // There will be (30 + 100) pages remaining in the store, and make sure their
  // files weren't cleared.
  EXPECT_EQ(1UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(130LL, store_test_util()->GetPageCount());
  EXPECT_EQ(130UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 5, 1);
}

TEST_F(ClearStorageTaskTest, TryClearPersistentPages) {
  Initialize({{kDownloadNamespace, 20, 0}});

  clock()->Advance(base::TimeDelta::FromDays(367));
  RunClearStorageTask(clock()->Now());

  // There's 20 pages and the clock advances for more than a year.
  // No pages should be deleted since they're all persistent pages.
  EXPECT_EQ(0UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(20LL, store_test_util()->GetPageCount());
  EXPECT_EQ(0UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(20UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 0);
}

TEST_F(ClearStorageTaskTest, TryClearPersistentPagesWithStoragePressure) {
  // Sets the free space with 1KB.
  Initialize({{kDownloadNamespace, 20, 0}});
  SetFreeSpace(1024);

  clock()->Advance(base::TimeDelta::FromDays(367));
  RunClearStorageTask(clock()->Now());

  // There're 20 pages and the clock advances for more than a year.
  // No pages should be deleted since they're all persistent pages.
  EXPECT_EQ(0UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(20LL, store_test_util()->GetPageCount());
  EXPECT_EQ(0UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(20UL, test_utils::GetFileCountInDirectory(PrivateDir()));
}

TEST_F(ClearStorageTaskTest, ClearMultipleTimes) {
  // Initializing with 20 unexpired and 0 expired pages in bookmark namespace,
  // 30 unexpired and 1 expired pages in last_n namespace, and 40 persistent
  // pages in download namespace.
  Initialize({{kBookmarkNamespace, 20, 0},
              {kLastNNamespace, 30, 1},
              {kDownloadNamespace, 40, 0}});

  // Check preconditions, especially that last_n expiration is longer than
  // bookmark's.
  const OfflinePageClientPolicy& bookmark_policy =
      GetPolicy(kBookmarkNamespace);
  const OfflinePageClientPolicy& last_n_policy = GetPolicy(kLastNNamespace);
  const OfflinePageClientPolicy& download_policy =
      GetPolicy(kDownloadNamespace);
  ASSERT_EQ(LifetimeType::TEMPORARY, bookmark_policy.lifetime_type);
  ASSERT_EQ(LifetimeType::TEMPORARY, last_n_policy.lifetime_type);
  ASSERT_EQ(LifetimeType::PERSISTENT, download_policy.lifetime_type);
  ASSERT_GT(last_n_policy.expiration_period, bookmark_policy.expiration_period);

  // Advance 30 minutes from initial pages creation time.
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  RunClearStorageTask(clock()->Now());

  // There's only 1 expired pages, so it will be cleared. There will be (30 +
  // 20 + 40) pages remaining, 40 of them are persistent pages.
  EXPECT_EQ(1UL, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(90LL, store_test_util()->GetPageCount());
  EXPECT_EQ(50UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 30, 1);

  // Advance the clock by the expiration period of bookmark namespace so that
  // all pages left in that namespace should be expired.
  clock()->Advance(bookmark_policy.expiration_period);
  RunClearStorageTask(clock()->Now());

  // All pages in bookmark namespace should be cleared. And only 70 pages
  // remaining after the clearing, 40 of them are persistent pages.
  EXPECT_EQ(20UL, last_cleared_page_count());
  EXPECT_EQ(2, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(70LL, store_test_util()->GetPageCount());
  EXPECT_EQ(30UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 21);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation",
      30 + bookmark_policy.expiration_period.InMinutes(), 20);

  // Advance the clock by 1 ms, there's no change in pages so the attempt to
  // clear storage should be unnecessary.
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  RunClearStorageTask(clock()->Now());

  // The clearing attempt is unnecessary.
  EXPECT_EQ(0UL, last_cleared_page_count());
  EXPECT_EQ(3, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(70LL, store_test_util()->GetPageCount());
  EXPECT_EQ(30UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 21);

  // Adding more fresh pages in last_n namespace to make storage usage exceed
  // limit, so even if only 5 minutes passed from last clearing, this will still
  // clear some pages.
  // Free storage space is 200MB and all temporary pages take 270 * 0.5MB =
  // 135MB, which is over (135MB + 200MB) * 0.3 = 100.5MB.
  // In order to bring the storage usage down to (135MB + 200MB) * 0.1 = 33.5MB,
  // (135MB - 33.5MB) needs to be released, which means 203 temporary pages need
  // to be cleared.
  AddPages({kLastNNamespace, 240, 0});
  SetFreeSpace(200 * (1 << 20));
  clock()->Advance(base::TimeDelta::FromMinutes(5));
  RunClearStorageTask(clock()->Now());

  // There should be 107 pages remaining after the clearing (including 40
  // persistent pages).
  EXPECT_EQ(203UL, last_cleared_page_count());
  EXPECT_EQ(4, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(107LL, store_test_util()->GetPageCount());
  EXPECT_EQ(67UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 224);
  // The 30 original ones last_n pages are cleared (and they fall into the same
  // bucket as the 20 from bookmarks)...
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation",
      30 + bookmark_policy.expiration_period.InMinutes() + 5, 20 + 30);
  // ... As well as 133 from this latest round.
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 5, 173);

  // Advance the clock by 300 days, in order to expire all temporary pages. Only
  // 67 temporary pages are left from the last clearing.
  clock()->Advance(base::TimeDelta::FromDays(300));
  RunClearStorageTask(clock()->Now());

  // All temporary pages should be cleared by now.
  EXPECT_EQ(67UL, last_cleared_page_count());
  EXPECT_EQ(5, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(40LL, store_test_util()->GetPageCount());
  EXPECT_EQ(0UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation", 291);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.ClearTemporaryPages.TimeSinceCreation",
      base::TimeDelta::FromDays(300).InMinutes() + 5, 67);
}

}  // namespace offline_pages
