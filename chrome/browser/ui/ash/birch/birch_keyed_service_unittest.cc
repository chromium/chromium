// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// TODO(https://crbug.com/1370774): move `ScopedTestMountPoint` out of holding
// space to remove the dependency on holding space code.
using ash::holding_space::ScopedTestMountPoint;

class BirchKeyedServiceTest : public BrowserWithTestWindowTest {
  // public testing::Test {
 public:
  BirchKeyedServiceTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        fake_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  void SetUp() override {
    switches::SetIgnoreBirchSecretKeyForTest(true);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    BrowserWithTestWindowTest::SetUp();

    file_suggest_service_ = static_cast<MockFileSuggestKeyedService*>(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            GetProfile()));

    mount_point_ = std::make_unique<ScopedTestMountPoint>(
        "test_mount", storage::kFileSystemTypeLocal,
        file_manager::VOLUME_TYPE_TESTING);
    mount_point_->Mount(GetProfile());

    birch_keyed_service_ =
        BirchKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  }

  void TearDown() override {
    mount_point_.reset();
    birch_keyed_service_ = nullptr;
    file_suggest_service_ = nullptr;
    fake_user_manager_.Reset();
    BrowserWithTestWindowTest::TearDown();
    switches::SetIgnoreBirchSecretKeyForTest(false);
  }

  void LogIn(const std::string& email) override {
    // TODO(crbug.com/1494005): merge into BrowserWithTestWindowTest.
    const AccountId account_id(AccountId::FromUserEmail(email));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    GetSessionControllerClient()->AddUserSession(email);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    return profile_manager()->CreateTestingProfile(profile_name,
                                                   GetTestingFactories());
  }

  TestSessionControllerClient* GetSessionControllerClient() {
    return ash_test_helper()->test_session_controller_client();
  }

  MockFileSuggestKeyedService* file_suggest_service() {
    return file_suggest_service_;
  }

  BirchKeyedService* birch_keyed_service() { return birch_keyed_service_; }

  ScopedTestMountPoint* mount_point() { return mount_point_.get(); }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{FileSuggestKeyedServiceFactory::GetInstance(),
             base::BindRepeating(
                 &MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService,
                 temp_dir_.GetPath())}};
  }

 private:
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<ScopedTestMountPoint> mount_point_;

  raw_ptr<MockFileSuggestKeyedService> file_suggest_service_ = nullptr;

  raw_ptr<BirchKeyedService> birch_keyed_service_ = nullptr;

  base::test::ScopedFeatureList feature_list_{features::kBirchFeature};
};

TEST_F(BirchKeyedServiceTest, BirchFileSuggestProvider) {
  WaitUntilFileSuggestServiceReady(
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          GetProfile()));

  EXPECT_EQ(Shell::Get()->birch_model()->GetFileSuggestItemsForTest().size(),
            0u);

  const base::FilePath file_path_1 = mount_point()->CreateArbitraryFile();
  const base::FilePath file_path_2 = mount_point()->CreateArbitraryFile();

  file_suggest_service()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*new_prediction_reason=*/std::nullopt,
           /*timestamp=*/std::nullopt,
           /*new_score=*/std::nullopt},
          {FileSuggestionType::kDriveFile, file_path_2,
           /*new_prediction_reason=*/std::nullopt,
           /*timestamp=*/std::nullopt,
           /*new_score=*/std::nullopt}});

  birch_keyed_service()
      ->GetFileSuggestProviderForTest()
      ->OnFileSuggestionUpdated(FileSuggestionType::kDriveFile);

  task_environment()->RunUntilIdle();

  // Check that the birch model now has two file suggestions.
  EXPECT_EQ(Shell::Get()->birch_model()->GetFileSuggestItemsForTest().size(),
            2u);
}

}  // namespace ash
