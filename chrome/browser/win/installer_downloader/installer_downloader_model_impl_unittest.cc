// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/win/cloud_synced_folder_checker.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "chrome/browser/win/installer_downloader/system_info_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::download::DownloadInterruptReason;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace installer_downloader {
namespace {

// Mock that lets the tests control OS / OneDrive probes.
class MockSystemInfoProvider : public SystemInfoProvider {
 public:
  MOCK_METHOD(bool, IsHardwareEligibleForWin11, (), (override));
  MOCK_METHOD(cloud_synced_folder_checker::CloudSyncStatus,
              EvaluateOneDriveSyncStatus,
              (),
              (override));
  MOCK_METHOD(bool, IsOsEligible, (), (override));
};

class InstallerDownloaderModelTest : public testing::Test {
 protected:
  InstallerDownloaderModelTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    auto mock_system_info_provider_ptr =
        std::make_unique<StrictMock<MockSystemInfoProvider>>();
    mock_system_info_provider_ = mock_system_info_provider_ptr.get();
    model_ = std::make_unique<InstallerDownloaderModelImpl>(
        std::move(mock_system_info_provider_ptr));
  }

  PrefService& GetLocalState() {
    return *TestingBrowserProcess::GetGlobal()->local_state();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<InstallerDownloaderModelImpl> model_;
  raw_ptr<MockSystemInfoProvider> mock_system_info_provider_;
  content::MockDownloadManager mock_download_manager_;
};

TEST_F(InstallerDownloaderModelTest, MaxShowCountNotExceeded) {
  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             false);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount - 1);
  EXPECT_TRUE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest, MaxShowCountExactlyAtLimit) {
  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             false);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount);
  EXPECT_FALSE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest, MaxShowCountAboveLimit) {
  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             false);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount + 1);
  EXPECT_FALSE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest,
       IncrementShowCountPersistsAndStopsAtLimit) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kInstallerDownloaderReengagement);

  // Start from a clean slate.
  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             false);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 0);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderTotalShowCount, 0);

  // Increment (kMaxShowCount-1) times and verify we have NOT hit the ceiling.
  for (int i = 0; i < InstallerDownloaderModelImpl::kMaxShowCount - 1; ++i) {
    model_->IncrementShowCount();
    EXPECT_TRUE(model_->CanShowInfobar());
    EXPECT_EQ(i + 1, GetLocalState().GetInteger(
                         prefs::kInstallerDownloaderInfobarShowCount));
  }

  // Not logged yet.
  histograms.ExpectTotalCount("Windows.InstallerDownloader.TotalShowCount", 0);

  // One more increment reaches the exact limit.
  model_->IncrementShowCount();
  EXPECT_FALSE(model_->CanShowInfobar());
  EXPECT_EQ(
      InstallerDownloaderModelImpl::kMaxShowCount,
      GetLocalState().GetInteger(prefs::kInstallerDownloaderInfobarShowCount));

  // Logged now.
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.TotalShowCount",
                                /*sample=*/3, /*expected_bucket_count=*/1);

  // Extra increments keep the model in "limit reached" state.
  model_->IncrementShowCount();
  EXPECT_FALSE(model_->CanShowInfobar());
  EXPECT_EQ(
      InstallerDownloaderModelImpl::kMaxShowCount + 1,
      GetLocalState().GetInteger(prefs::kInstallerDownloaderInfobarShowCount));

  // Still logged only once.
  histograms.ExpectTotalCount("Windows.InstallerDownloader.TotalShowCount", 1);
}

TEST_F(InstallerDownloaderModelTest, PreventFutureDisplayPrefBlocksInfobar) {
  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             true);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 0);
  EXPECT_FALSE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest, PreventFutureDisplayMethodWorks) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kInstallerDownloaderReengagement);

  EXPECT_FALSE(GetLocalState().GetBoolean(
      prefs::kInstallerDownloaderPreventFutureDisplay));

  GetLocalState().SetInteger(prefs::kInstallerDownloaderTotalShowCount, 1);

  model_->PreventFutureDisplay();

  EXPECT_TRUE(GetLocalState().GetBoolean(
      prefs::kInstallerDownloaderPreventFutureDisplay));
  EXPECT_FALSE(model_->CanShowInfobar());

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.TotalShowCount",
                                /*sample=*/1, /*expected_bucket_count=*/1);
}

TEST_F(InstallerDownloaderModelTest,
       ReengagementEnabledBypassesPreventFutureDisplayAfterCooldown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             true);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 1);
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(61));

  EXPECT_TRUE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest,
       ReengagementEnabledRespectsCooldownPeriodBetweenCycles) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             true);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 1);
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(10));

  EXPECT_FALSE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest,
       ReengagementEnabledStartsNewCycleAfterCooldown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             false);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 3);
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(61));

  EXPECT_TRUE(model_->CanShowInfobar());
  model_->IncrementShowCount();
  EXPECT_EQ(2,
            GetLocalState().GetInteger(prefs::kInstallerDownloaderCycleCount));
  EXPECT_EQ(1, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));
  EXPECT_FALSE(GetLocalState().GetBoolean(
      prefs::kInstallerDownloaderPreventFutureDisplay));
}

TEST_F(InstallerDownloaderModelTest,
       ReengagementEnabledAllowsUpToThreeShowsWithinCycle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  // User is in Cycle 2.
  GetLocalState().SetInteger(prefs::kInstallerDownloaderCycleCount, 2);
  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             false);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 1);

  // Show 1 in Cycle 2 was shown. Still eligible for Show 2 in this cycle.
  EXPECT_TRUE(model_->CanShowInfobar());
  model_->IncrementShowCount();
  EXPECT_EQ(2,
            GetLocalState().GetInteger(prefs::kInstallerDownloaderCycleCount));
  EXPECT_EQ(2, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));

  // Still eligible for Show 3 in this cycle.
  EXPECT_TRUE(model_->CanShowInfobar());
  model_->IncrementShowCount();
  EXPECT_EQ(2,
            GetLocalState().GetInteger(prefs::kInstallerDownloaderCycleCount));
  EXPECT_EQ(3, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));

  // Cycle 2 is now finished (reached limit of 3 shows). Cannot show
  // immediately.
  EXPECT_FALSE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest, ReengagementEnabledRespectsMaxCycleCount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             true);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderCycleCount, 3);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 1);
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(61));

  EXPECT_FALSE(model_->CanShowInfobar());
}

TEST_F(InstallerDownloaderModelTest,
       DownloadCompletedPermanentlyStopsDisplays) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  GetLocalState().SetInteger(prefs::kInstallerDownloaderTotalShowCount, 2);

  GetLocalState().SetBoolean(prefs::kInstallerDownloaderPreventFutureDisplay,
                             true);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 1);
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(61));

  EXPECT_TRUE(model_->CanShowInfobar());
  model_->RecordDownloadCompleted();
  EXPECT_FALSE(model_->CanShowInfobar());

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.TotalShowCount",
                                /*sample=*/2, /*expected_bucket_count=*/1);
}

TEST_F(InstallerDownloaderModelTest, TotalShowCountIncrementedAcrossCycles) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  // Start from clean slate.
  GetLocalState().SetInteger(prefs::kInstallerDownloaderTotalShowCount, 0);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount, 0);
  GetLocalState().SetInteger(prefs::kInstallerDownloaderCycleCount, 0);

  // Cycle 1, Show 1
  model_->IncrementShowCount();
  EXPECT_EQ(
      1, GetLocalState().GetInteger(prefs::kInstallerDownloaderTotalShowCount));
  EXPECT_EQ(1, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));

  // Cycle 1, Show 2
  model_->IncrementShowCount();
  EXPECT_EQ(
      2, GetLocalState().GetInteger(prefs::kInstallerDownloaderTotalShowCount));
  EXPECT_EQ(2, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));

  // Simulate cycle finish (dismissed)
  model_->PreventFutureDisplay();
  histograms.ExpectTotalCount("Windows.InstallerDownloader.TotalShowCount", 0);

  // Fast forward past cooldown
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(61));

  // Cycle 2, Show 1 (which is total show 3)
  EXPECT_TRUE(model_->CanShowInfobar());
  model_->IncrementShowCount();
  EXPECT_EQ(
      3, GetLocalState().GetInteger(prefs::kInstallerDownloaderTotalShowCount));
  EXPECT_EQ(1, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));
  EXPECT_EQ(2,
            GetLocalState().GetInteger(prefs::kInstallerDownloaderCycleCount));

  // Cycle 2, Show 2 (total show 4)
  model_->IncrementShowCount();
  // Cycle 2, Show 3 (total show 5)
  model_->IncrementShowCount();
  EXPECT_FALSE(model_->CanShowInfobar());
  histograms.ExpectTotalCount("Windows.InstallerDownloader.TotalShowCount", 0);

  // Fast forward past cooldown
  GetLocalState().SetTime(prefs::kInstallerDownloaderInfobarLastShowTime,
                          base::Time::Now() - base::Days(61));

  // Cycle 3, Show 1 (total show 6)
  EXPECT_TRUE(model_->CanShowInfobar());
  model_->IncrementShowCount();
  EXPECT_EQ(
      6, GetLocalState().GetInteger(prefs::kInstallerDownloaderTotalShowCount));
  EXPECT_EQ(1, GetLocalState().GetInteger(
                   prefs::kInstallerDownloaderInfobarShowCount));
  EXPECT_EQ(3,
            GetLocalState().GetInteger(prefs::kInstallerDownloaderCycleCount));

  // Cycle 3, Show 2 (total show 7)
  model_->IncrementShowCount();
  // Cycle 3, Show 3 (total show 8) - exhausted
  model_->IncrementShowCount();
  EXPECT_FALSE(model_->CanShowInfobar());

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.TotalShowCount",
                                /*sample=*/8, /*expected_bucket_count=*/1);
}

// This test verifies that when the Os version is ineligible, no additional
// check or call should be done. The destination path should be empty.
TEST_F(InstallerDownloaderModelTest, NotEligibleWhenOsIneligible) {
  EXPECT_CALL(*mock_system_info_provider_, IsOsEligible())
      .WillOnce(Return(false));

  base::RunLoop run_loop;
  model_->CheckEligibility(base::BindLambdaForTesting(
      [&](std::optional<base::FilePath> destination) {
        EXPECT_FALSE(destination.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test verifies that when the Os version is eligible for the Installer
// Download, the next checks should be performed. If the Os meet the
// requirements for an Win11 upgrade, then no additional check is needed. The
// destination path should be empty.
TEST_F(InstallerDownloaderModelTest, OsUpgradeEligible) {
  EXPECT_CALL(*mock_system_info_provider_, IsOsEligible())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_system_info_provider_, IsHardwareEligibleForWin11())
      .WillOnce(Return(true));

  base::RunLoop run_loop;
  model_->CheckEligibility(base::BindLambdaForTesting(
      [&](std::optional<base::FilePath> destination) {
        EXPECT_FALSE(destination.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test verifies that when the Os version is eligible for the Installer
// Download, the next checks should be performed. If the Os do not meet the
// requirements for an Win11 upgrade, the cloud storage folder should be checked
// so that the installer destination can be determined. The determination can
// fall in one of the following cases:
// 1. No cloud storage folder exist or it's not synced.
// 2. Cloud storage folder exist, it's synced, and the desktop folder is also
//    synced.
// 3. Cloud storage folder exist, it's synced, and the desktop folder is not
//    synced.
TEST_F(InstallerDownloaderModelTest, OsUpgradeNotEligibleWhenNoPathSet) {
  EXPECT_CALL(*mock_system_info_provider_, IsOsEligible())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_system_info_provider_, IsHardwareEligibleForWin11())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_system_info_provider_, EvaluateOneDriveSyncStatus())
      .WillOnce(Return(cloud_synced_folder_checker::CloudSyncStatus()));

  base::RunLoop run_loop;
  model_->CheckEligibility(base::BindLambdaForTesting(
      [&](std::optional<base::FilePath> destination) {
        EXPECT_FALSE(destination.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InstallerDownloaderModelTest, OsUpgradeNotEligibleWhenDesktopPathSet) {
  cloud_synced_folder_checker::CloudSyncStatus status;
  status.desktop_path =
      base::FilePath(FILE_PATH_LITERAL("C:\\storage\\desktop"));
  status.one_drive_path = base::FilePath(FILE_PATH_LITERAL("C:\\storage"));

  EXPECT_CALL(*mock_system_info_provider_, IsOsEligible())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_system_info_provider_, IsHardwareEligibleForWin11())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_system_info_provider_, EvaluateOneDriveSyncStatus())
      .WillOnce(Return(status));

  base::RunLoop run_loop;
  model_->CheckEligibility(base::BindLambdaForTesting(
      [&](std::optional<base::FilePath> destination) {
        ASSERT_TRUE(destination.has_value());
        EXPECT_EQ(destination.value(), status.desktop_path);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InstallerDownloaderModelTest, OsUpgradeNotEligibleWhenOnlyRootPathSet) {
  cloud_synced_folder_checker::CloudSyncStatus status;
  status.one_drive_path = base::FilePath(FILE_PATH_LITERAL("C:\\storage"));

  EXPECT_CALL(*mock_system_info_provider_, IsOsEligible())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_system_info_provider_, IsHardwareEligibleForWin11())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_system_info_provider_, EvaluateOneDriveSyncStatus())
      .WillOnce(Return(status));

  base::RunLoop run_loop;
  model_->CheckEligibility(base::BindLambdaForTesting(
      [&](std::optional<base::FilePath> destination) {
        EXPECT_TRUE(destination.has_value());
        EXPECT_EQ(destination, status.one_drive_path);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InstallerDownloaderModelTest, StartDownloadFailureInvokesCallback) {
  const base::FilePath destination(
      FILE_PATH_LITERAL("C:\\temp\\installer.exe"));
  const GURL url("https://example.com/installer.exe");

  base::RunLoop run_loop;
  EXPECT_CALL(mock_download_manager_, DownloadUrlMock(_))
      .WillOnce([&](download::DownloadUrlParameters* params) {
        std::move(params->callback())
            .Run(nullptr, DownloadInterruptReason::
                              DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
      });

  model_->StartDownload(url, destination, mock_download_manager_,
                        base::BindLambdaForTesting([&](bool succeeded) {
                          EXPECT_FALSE(succeeded);
                          run_loop.Quit();
                        }));

  run_loop.Run();
}

TEST_F(InstallerDownloaderModelTest, CompleteDownloadSuccessInvokesCallback) {
  const base::FilePath destination(
      FILE_PATH_LITERAL("C:\\temp\\installer.exe"));
  const GURL url("https://example.com/installer.exe");

  content::FakeDownloadItem fake_download_item;
  fake_download_item.SetDummyFilePath(destination);

  base::RunLoop run_loop;

  EXPECT_CALL(mock_download_manager_, DownloadUrlMock(_))
      .WillOnce([&](download::DownloadUrlParameters* params) {
        std::move(params->callback())
            .Run(&fake_download_item,
                 DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE);
      });

  model_->StartDownload(url, destination, mock_download_manager_,
                        base::BindLambdaForTesting([&](bool succeeded) {
                          EXPECT_TRUE(succeeded);
                          run_loop.Quit();
                        }));

  fake_download_item.SetIsDone(true);
  fake_download_item.SetState(download::DownloadItem::COMPLETE);
  fake_download_item.NotifyDownloadUpdated();

  run_loop.Run();
}

TEST_F(InstallerDownloaderModelTest, CompleteDownloadFailureInvokesCallback) {
  const base::FilePath destination(
      FILE_PATH_LITERAL("C:\\temp\\installer.exe"));
  const GURL url("https://example.com/installer.exe");

  content::FakeDownloadItem fake_download_item;
  fake_download_item.SetDummyFilePath(destination);

  base::RunLoop run_loop;

  EXPECT_CALL(mock_download_manager_, DownloadUrlMock(_))
      .WillOnce([&](download::DownloadUrlParameters* params) {
        std::move(params->callback())
            .Run(&fake_download_item,
                 DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE);
      });

  model_->StartDownload(url, destination, mock_download_manager_,
                        base::BindLambdaForTesting([&](bool succeeded) {
                          EXPECT_FALSE(succeeded);
                          run_loop.Quit();
                        }));

  fake_download_item.SetIsDone(true);
  fake_download_item.SetState(download::DownloadItem::CANCELLED);
  fake_download_item.NotifyDownloadUpdated();

  run_loop.Run();
}

TEST_F(InstallerDownloaderModelTest, DestinationMatchMetricTrue) {
  base::HistogramTester histograms;

  const base::FilePath destination(FILE_PATH_LITERAL("C:\\tmp\\installer.exe"));
  const GURL url("https://example.com/installer.exe");
  content::FakeDownloadItem fake_item;

  // The observer compares against the target file path.
  fake_item.SetTargetFilePath(destination);
  fake_item.SetState(download::DownloadItem::IN_PROGRESS);

  EXPECT_CALL(mock_download_manager_, DownloadUrlMock(_))
      .WillOnce([&](download::DownloadUrlParameters* params) {
        std::move(params->callback())
            .Run(&fake_item, download::DOWNLOAD_INTERRUPT_REASON_NONE);
      });

  model_->StartDownload(url, destination, mock_download_manager_,
                        base::DoNothing());

  fake_item.SetState(download::DownloadItem::COMPLETE);
  fake_item.NotifyDownloadUpdated();

  histograms.ExpectUniqueSample(
      "Windows.InstallerDownloader.DestinationMatches",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(InstallerDownloaderModelTest, DestinationMatchMetricFalse) {
  base::HistogramTester histograms;

  const base::FilePath requested(FILE_PATH_LITERAL("C:\\tmp\\installer.exe"));
  const base::FilePath actual(FILE_PATH_LITERAL("C:\\tmp\\installer (1).exe"));
  const GURL url("https://example.com/installer.exe");

  content::FakeDownloadItem fake_item;
  // The actual path is different from what was requested, simulating a
  // path change by the download manager.
  fake_item.SetTargetFilePath(actual);
  fake_item.SetState(download::DownloadItem::IN_PROGRESS);

  EXPECT_CALL(mock_download_manager_, DownloadUrlMock(_))
      .WillOnce([&](download::DownloadUrlParameters* params) {
        std::move(params->callback())
            .Run(&fake_item, download::DOWNLOAD_INTERRUPT_REASON_NONE);
      });

  model_->StartDownload(url, requested, mock_download_manager_,
                        base::DoNothing());

  fake_item.SetState(download::DownloadItem::COMPLETE);
  fake_item.NotifyDownloadUpdated();

  histograms.ExpectUniqueSample(
      "Windows.InstallerDownloader.DestinationMatches",
      /*sample=*/false, /*expected_bucket_count=*/1);
}

TEST_F(InstallerDownloaderModelTest, IncrementShowCountUpdatesLastShownTime) {
  EXPECT_TRUE(GetLocalState()
                  .GetTime(prefs::kInstallerDownloaderInfobarLastShowTime)
                  .is_null());

  // First increment should record the current time.
  model_->IncrementShowCount();
  const base::Time time1 =
      GetLocalState().GetTime(prefs::kInstallerDownloaderInfobarLastShowTime);
  EXPECT_FALSE(time1.is_null());
  EXPECT_EQ(time1, base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(30));

  // Second increment should record the new, later time.
  model_->IncrementShowCount();
  const base::Time time2 =
      GetLocalState().GetTime(prefs::kInstallerDownloaderInfobarLastShowTime);
  EXPECT_FALSE(time2.is_null());

  EXPECT_GT(time2, time1);
  EXPECT_EQ(time2, time1 + base::Seconds(30));
}

TEST_F(InstallerDownloaderModelTest,
       TotalShowCountLoggedOnDismissalInLastCycle) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kInstallerDownloaderReengagement,
      {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}});

  // Cycle 1: 3 ignores.
  model_->IncrementShowCount();
  model_->IncrementShowCount();
  model_->IncrementShowCount();

  // Cycle 2: 1 show, then dismiss.
  task_environment_.FastForwardBy(base::Days(61));
  model_->IncrementShowCount();    // Starts Cycle 2.
  model_->PreventFutureDisplay();  // Dismisses Cycle 2.
  // Cycle 2 finished. Not logged yet.
  histograms.ExpectTotalCount("Windows.InstallerDownloader.TotalShowCount", 0);

  // Cycle 3: 1 show, then dismiss.
  task_environment_.FastForwardBy(base::Days(61));
  model_->IncrementShowCount();  // Starts Cycle 3 (last cycle).

  // Not logged yet.
  histograms.ExpectTotalCount("Windows.InstallerDownloader.TotalShowCount", 0);

  model_->PreventFutureDisplay();  // Dismisses Cycle 3 (last cycle).

  // Logged now with value 5 (3 from C1, 1 from C2, 1 from C3).
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.TotalShowCount",
                                /*sample=*/5, /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace installer_downloader
