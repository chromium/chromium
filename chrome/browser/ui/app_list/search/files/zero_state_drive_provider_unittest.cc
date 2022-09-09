// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_drive_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

class TestFileSuggestKeyedService : public FileSuggestKeyedService {
 public:
  explicit TestFileSuggestKeyedService(Profile* profile)
      : FileSuggestKeyedService(profile) {}
  TestFileSuggestKeyedService(const TestFileSuggestKeyedService&) = delete;
  TestFileSuggestKeyedService& operator=(TestFileSuggestKeyedService&) = delete;
  ~TestFileSuggestKeyedService() override = default;

  // FileSuggestKeyedService:
  void MaybeUpdateItemSuggestCache(
      base::PassKey<ZeroStateDriveProvider>) override {
    update_count_++;
  }

  int update_count_ = 0;
};

std::unique_ptr<KeyedService> BuildTestFileSuggestKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<TestFileSuggestKeyedService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class ZeroStateDriveProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "primary_profile@test",
        {{FileSuggestKeyedServiceFactory::GetInstance(),
          base::BindRepeating(&BuildTestFileSuggestKeyedService)}});
    file_suggest_service_ = static_cast<TestFileSuggestKeyedService*>(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_));
    session_manager_ = std::make_unique<session_manager::SessionManager>();

    provider_ = std::make_unique<ZeroStateDriveProvider>(
        profile_, nullptr,
        drive::DriveIntegrationServiceFactory::GetForProfile(profile_),
        session_manager_.get());
  }

  void FastForwardByMinutes(int minutes) {
    task_environment_.FastForwardBy(base::Minutes(minutes));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  int update_count() const { return file_suggest_service_->update_count_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ZeroStateDriveProvider> provider_;
  base::HistogramTester histogram_tester_;
  TestFileSuggestKeyedService* file_suggest_service_ = nullptr;
};

// TODO(crbug.com/1348339): Add a test for a file mount-triggered update at
// construction time.

// Test that each of the trigger events causes an update.
TEST_F(ZeroStateDriveProviderTest, UpdateCache) {
  // Fast forward past the construction delay.
  FastForwardByMinutes(1);
  EXPECT_EQ(update_count(), 0);

  provider_->OnFileSystemMounted();
  // File system mount updates are posted with a delay, so fast forward here.
  FastForwardByMinutes(1);
  EXPECT_EQ(update_count(), 1);

  provider_->ViewClosing();
  EXPECT_EQ(update_count(), 2);

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(update_count(), 3);

  power_manager::ScreenIdleState idle_state;
  idle_state.set_dimmed(false);
  idle_state.set_off(false);
  provider_->ScreenIdleStateChanged(idle_state);
  EXPECT_EQ(update_count(), 4);
}

// Test that an update is triggered when the screen turns on.
TEST_F(ZeroStateDriveProviderTest, UpdateOnWake) {
  // Fast forward past the construction delay.
  FastForwardByMinutes(1);

  power_manager::ScreenIdleState idle_state;
  EXPECT_EQ(update_count(), 0);

  // Turn the screen on. This logs a query since the screen state is default off
  // when the provider is initialized.
  idle_state.set_dimmed(false);
  idle_state.set_off(false);
  provider_->ScreenIdleStateChanged(idle_state);
  EXPECT_EQ(update_count(), 1);

  // Dim the screen.
  idle_state.set_dimmed(true);
  provider_->ScreenIdleStateChanged(idle_state);
  EXPECT_EQ(update_count(), 1);

  // Undim the screen. This should NOT log a query.
  idle_state.set_dimmed(false);
  provider_->ScreenIdleStateChanged(idle_state);
  EXPECT_EQ(update_count(), 1);

  // Turn off the screen.
  idle_state.set_dimmed(true);
  idle_state.set_off(true);
  provider_->ScreenIdleStateChanged(idle_state);
  EXPECT_EQ(update_count(), 1);

  // Turn on the screen. This logs a query.
  idle_state.set_dimmed(false);
  idle_state.set_off(false);
  provider_->ScreenIdleStateChanged(idle_state);
  EXPECT_EQ(update_count(), 2);
}

}  // namespace app_list
