// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/startup_maintenance_task.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const int64_t kTestFileSize = 512 * (1 << 10);  // Default file size is 512KB.

// Used for checking if the page is still present in database and/or filesystem.
enum class PagePresence {
  BOTH_DB_AND_FILESYSTEM,
  DB_ONLY,
  FILESYSTEM_ONLY,
  NONE,
};

}  // namespace

class StartupMaintenanceTaskTest : public ModelTaskTestBase {
 public:
  StartupMaintenanceTaskTest();
  ~StartupMaintenanceTaskTest() override;

  void SetUp() override;
  PagePresence CheckPagePresence(const OfflinePageItem& page);

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

StartupMaintenanceTaskTest::StartupMaintenanceTaskTest() {}

StartupMaintenanceTaskTest::~StartupMaintenanceTaskTest() {}

void StartupMaintenanceTaskTest::SetUp() {
  ModelTaskTestBase::SetUp();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

PagePresence StartupMaintenanceTaskTest::CheckPagePresence(
    const OfflinePageItem& page) {
  if (base::PathExists(page.file_path)) {
    if (store_test_util()->GetPageByOfflineId(page.offline_id))
      return PagePresence::BOTH_DB_AND_FILESYSTEM;
    else
      return PagePresence::FILESYSTEM_ONLY;
  } else {
    if (store_test_util()->GetPageByOfflineId(page.offline_id))
      return PagePresence::DB_ONLY;
    else
      return PagePresence::NONE;
  }
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  DISABLED_TestDeletePageInLegacyArchivesDir
#else
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  TestDeletePageInLegacyArchivesDir
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_TestDeletePageInLegacyArchivesDir) {
  // |temporary_page| will be removed since it's temporary and its archive file
  // is in private directory.
  // |persistent_page| will not be affected by the maintenance task.
  generator()->SetArchiveDirectory(PrivateDir());
  generator()->SetNamespace(kLastNNamespace);
  OfflinePageItem temporary_page1 = AddPage();
  OfflinePageItem temporary_page2 = AddPageWithoutDBEntry();
  generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem persistent_page1 = AddPage();
  OfflinePageItem persistent_page2 = AddPageWithoutDBEntry();

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(4UL, test_utils::GetFileCountInDirectory(PrivateDir()));

  auto task =
      std::make_unique<StartupMaintenanceTask>(store(), archive_manager());
  RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(persistent_page2));
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Legacy.DeletedHeadlessFileCount", 2, 1);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteFileWithoutDbEntry DISABLED_TestDeleteFileWithoutDbEntry
#else
#define MAYBE_TestDeleteFileWithoutDbEntry TestDeleteFileWithoutDbEntry
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_TestDeleteFileWithoutDbEntry) {
  // |temporary_page1| will not be affected.
  // |temporary_page2| will have the file deleted since the file doesn't have a
  // DB entry.
  // |persistent_page1| will not be affected.
  // |persistent_page2| will have its file deleted, since the file is in private
  // directory and has no associated DB entry.
  generator()->SetNamespace(kLastNNamespace);
  generator()->SetArchiveDirectory(TemporaryDir());
  OfflinePageItem temporary_page1 = AddPage();
  OfflinePageItem temporary_page2 = AddPageWithoutDBEntry();

  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem persistent_page1 = AddPage();
  OfflinePageItem persistent_page2 = AddPageWithoutDBEntry();

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(persistent_page2));

  auto task =
      std::make_unique<StartupMaintenanceTask>(store(), archive_manager());
  RunTask(std::move(task));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(temporary_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(persistent_page2));

  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Legacy.DeletedHeadlessFileCount", 1, 1);
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount",
      0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeleteDbEntryWithoutFile DISABLED_TestDeleteDbEntryWithoutFile
#else
#define MAYBE_TestDeleteDbEntryWithoutFile TestDeleteDbEntryWithoutFile
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_TestDeleteDbEntryWithoutFile) {
  // |temporary_page1| will not be affected.
  // |temporary_page2| will be deleted from DB since it has no file associated.
  // |persistent_page1| will not be affected.
  // |persistent_page2| will be marked as file_missing.
  generator()->SetNamespace(kLastNNamespace);
  generator()->SetArchiveDirectory(TemporaryDir());
  OfflinePageItem temporary_page1 = AddPage();
  OfflinePageItem temporary_page2 = AddPageWithoutFile();

  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem persistent_page1 = AddPage();
  OfflinePageItem persistent_page2 = AddPageWithoutFile();

  EXPECT_EQ(4LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(persistent_page2));

  auto task =
      std::make_unique<StartupMaintenanceTask>(store(), archive_manager());
  RunTask(std::move(task));

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(temporary_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page1));
  EXPECT_EQ(PagePresence::DB_ONLY, CheckPagePresence(persistent_page2));

  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 0);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount", 1,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_CombinedTest DISABLED_CombinedTest
#else
#define MAYBE_CombinedTest CombinedTest
#endif
TEST_F(StartupMaintenanceTaskTest, MAYBE_CombinedTest) {
  // Adding a bunch of pages with different setups for temporary pages.
  // |temporary_page1| will not be affected.
  // |temporary_page{2,3,4,5,6}| will be deleted.
  generator()->SetNamespace(kLastNNamespace);
  generator()->SetArchiveDirectory(TemporaryDir());
  OfflinePageItem temporary_page1 = AddPage();
  OfflinePageItem temporary_page2 = AddPageWithoutDBEntry();
  OfflinePageItem temporary_page3 = AddPageWithoutFile();
  generator()->SetArchiveDirectory(PrivateDir());
  OfflinePageItem temporary_page4 = AddPage();
  OfflinePageItem temporary_page5 = AddPageWithoutDBEntry();
  OfflinePageItem temporary_page6 = AddPageWithoutFile();
  // Adding a bunch of pages with different setups for persistent pages.
  // |persistent_page1| will not be affected.
  // |persistent_page2| will be deleted from Filesystem, since it's in private
  // directory.
  generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem persistent_page1 = AddPage();
  OfflinePageItem persistent_page2 = AddPageWithoutDBEntry();

  EXPECT_EQ(5LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(4UL, test_utils::GetFileCountInDirectory(PrivateDir()));

  auto task =
      std::make_unique<StartupMaintenanceTask>(store(), archive_manager());
  RunTask(std::move(task));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(PrivateDir()));

  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(temporary_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page2));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page3));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page4));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page5));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(temporary_page6));

  EXPECT_EQ(PagePresence::BOTH_DB_AND_FILESYSTEM,
            CheckPagePresence(persistent_page1));
  EXPECT_EQ(PagePresence::NONE, CheckPagePresence(persistent_page2));

  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Legacy.DeletedHeadlessFileCount", 2, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingArchiveFileCount", 2,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.PagesMissingDbEntryCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ConsistencyCheck.Temporary.Result",
      static_cast<int>(SyncOperationResult::SUCCESS), 1);
}

TEST_F(StartupMaintenanceTaskTest, TestKeepingNonMhtmlFile) {
  // Create a persistent offline page with mhtml extension but has no DB entry.
  // It will not be deleted since it's in public directory.
  generator()->SetNamespace(kDownloadNamespace);
  generator()->SetArchiveDirectory(PublicDir());
  OfflinePageItem page1 = AddPageWithoutDBEntry();
  // Create a file with non-mhtml extension. It should not be affected.
  base::FilePath path;
  base::CreateTemporaryFileInDir(PublicDir(), &path);
  base::FilePath mp3_path = path.AddExtension(FILE_PATH_LITERAL("mp3"));
  EXPECT_TRUE(base::Move(path, mp3_path));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PublicDir()));

  auto task =
      std::make_unique<StartupMaintenanceTask>(store(), archive_manager());
  RunTask(std::move(task));

  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(PublicDir()));
  EXPECT_EQ(PagePresence::FILESYSTEM_ONLY, CheckPagePresence(page1));
}

TEST_F(StartupMaintenanceTaskTest, TestReportStorageUsage) {
  generator()->SetFileSize(kTestFileSize);
  const std::vector<std::string>& namespaces = GetAllPolicyNamespaces();

  // Adding pages into each namespace.
  for (const auto& name_space : namespaces) {
    // Set the correct namespace for generated pages, also put them into the
    // correct directories, otherwise they might be cleaned based on consistency
    // check.
    generator()->SetNamespace(name_space);
    if (GetPolicy(name_space).lifetime_type == LifetimeType::TEMPORARY)
      generator()->SetArchiveDirectory(TemporaryDir());
    else
      generator()->SetArchiveDirectory(PrivateDir());

    // For each namespace, insert pages based on the length of the namespace, so
    // that we don't need to have const values here.
    for (size_t count = 0; count < name_space.length(); ++count)
      AddPage();
  }

  auto task =
      std::make_unique<StartupMaintenanceTask>(store(), archive_manager());
  RunTask(std::move(task));

  // For each namespace, check if the storage usage was correctly reported,
  // since the value is reported with KiB as unit, divide it by 1024 here.
  for (const auto& name_space : namespaces) {
    histogram_tester()->ExpectUniqueSample(
        "OfflinePages.ClearStoragePreRunUsage2." + name_space,
        name_space.length() * kTestFileSize / 1024, 1);
  }
}

}  // namespace offline_pages
