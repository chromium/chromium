// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_drive_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

class TestItemSuggestCache : public ItemSuggestCache {
 public:
  explicit TestItemSuggestCache(Profile* profile)
      : ItemSuggestCache(profile, nullptr) {}
  TestItemSuggestCache(const TestItemSuggestCache&) = delete;
  TestItemSuggestCache& operator=(const TestItemSuggestCache&) = delete;
  ~TestItemSuggestCache() override = default;

  void UpdateCache() override { update_count_++; }

  int update_count_ = 0;
};

}  // namespace

class ZeroStateDriveProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    auto item_suggest_cache =
        std::make_unique<TestItemSuggestCache>(profile_.get());
    item_suggest_cache_ = item_suggest_cache.get();

    provider_ = std::make_unique<ZeroStateDriveProvider>(
        profile_.get(), nullptr,
        drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get()),
        session_manager_.get(), std::move(item_suggest_cache));
  }

  void FastForwardByMinutes(int minutes) {
    task_environment_.FastForwardBy(base::Minutes(minutes));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  int update_count() const { return item_suggest_cache_->update_count_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ZeroStateDriveProvider> provider_;
  base::HistogramTester histogram_tester_;

  // Owned by |provider_|.
  TestItemSuggestCache* item_suggest_cache_;
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
