// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/win/cloud_synced_folder_checker.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "chrome/browser/win/installer_downloader/system_info_provider.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  InstallerDownloaderModelTest() {
    auto mock_system_info_provider_ptr =
        std::make_unique<StrictMock<MockSystemInfoProvider>>();
    mock_system_info_provider_ = mock_system_info_provider_ptr.get();
    model_ = std::make_unique<InstallerDownloaderModelImpl>(
        std::move(mock_system_info_provider_ptr));
  }

  TestingPrefServiceSimple& GetLocalState() { return *local_state_.Get(); }

  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<InstallerDownloaderModelImpl> model_;
  raw_ptr<MockSystemInfoProvider> mock_system_info_provider_;
};

TEST_F(InstallerDownloaderModelTest, MaxShowCountNotExceeded) {
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount - 1);
  EXPECT_FALSE(model_->IsMaxShowCountReached());
}

TEST_F(InstallerDownloaderModelTest, MaxShowCountExactlyAtLimit) {
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount);
  EXPECT_TRUE(model_->IsMaxShowCountReached());
}

TEST_F(InstallerDownloaderModelTest, MaxShowCountAboveLimit) {
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount + 1);
  EXPECT_TRUE(model_->IsMaxShowCountReached());
}

// This test verifies that when the Os version is ineligible, no additional
// check or call should be done. The destination path should be empty.
TEST_F(InstallerDownloaderModelTest, NotEligibleWhenOsIneligible) {
  EXPECT_CALL(*mock_system_info_provider_, IsOsEligible())
      .WillOnce(Return(false));

  base::RunLoop run_loop;
  model_->CheckEligibility(base::BindLambdaForTesting(
      [&](const std::optional<base::FilePath>& destination) {
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
      [&](const std::optional<base::FilePath>& destination) {
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
      [&](const std::optional<base::FilePath>& destination) {
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
      [&](const std::optional<base::FilePath>& destination) {
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
      [&](const std::optional<base::FilePath>& destination) {
        EXPECT_TRUE(destination.has_value());
        EXPECT_EQ(destination, status.one_drive_path);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace installer_downloader
