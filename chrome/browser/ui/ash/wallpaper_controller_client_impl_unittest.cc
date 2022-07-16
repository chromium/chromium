// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/value_store/testing_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kChromeAppDailyRefreshInfoKey[] = "daily-refresh-info-key";

class WallpaperControllerClientImplTest : public testing::Test {
 public:
  WallpaperControllerClientImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  WallpaperControllerClientImplTest(const WallpaperControllerClientImplTest&) =
      delete;
  WallpaperControllerClientImplTest& operator=(
      const WallpaperControllerClientImplTest&) = delete;

  ~WallpaperControllerClientImplTest() override = default;

 private:
  ScopedTestingLocalState local_state_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
};

TEST_F(WallpaperControllerClientImplTest, Construction) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  // Singleton was initialized.
  EXPECT_EQ(&client, WallpaperControllerClientImpl::Get());

  // Object was set as client.
  EXPECT_TRUE(controller.was_client_set());
}

TEST_F(WallpaperControllerClientImplTest, MigrateCollectionIdFromValueStore) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  value_store::TestingValueStore value_store;

  // There is also a resume token and an enabled state, but that's not what is
  // being tested here, so only populate collectionId.
  std::string json("{\"collectionId\" : \"fun_collection\"}");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("fake@test.com", "444444");
  client.MigrateCollectionIdFromValueStoreForTesting(account_id, &value_store);

  EXPECT_EQ("fun_collection", controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNoCollectionId) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  value_store::TestingValueStore value_store;

  // There is also a resume token and an enabled state, but that's not what is
  // being tested here, so only populate collectionId.
  std::string json("{\"collectionId\" : null}");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("fake@test.com", "444444");
  client.MigrateCollectionIdFromValueStoreForTesting(account_id, &value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNoStore) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  AccountId account_id =
      AccountId::FromUserEmailGaiaId("fake@test.com", "444444");
  client.MigrateCollectionIdFromValueStoreForTesting(account_id, nullptr);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNotOKStatusCode) {
  using StatusCode = value_store::ValueStore::StatusCode;

  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  value_store::TestingValueStore value_store;
  value_store.set_status_code(StatusCode::OTHER_ERROR);

  // There is also a resume token and an enabled state, but that's not what is
  // being tested here, so only populate collectionId.
  std::string json("{\"collectionId\" : \"fun_collection\"}");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("fake@test.com", "444444");
  client.MigrateCollectionIdFromValueStoreForTesting(account_id, &value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNoPref) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  value_store::TestingValueStore value_store;
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("fake@test.com", "444444");
  client.MigrateCollectionIdFromValueStoreForTesting(account_id, &value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreMalformedPref) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  value_store::TestingValueStore value_store;
  std::string json("{");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("fake@test.com", "444444");
  client.MigrateCollectionIdFromValueStoreForTesting(account_id, &value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest, IsWallpaperSyncEnabledNoProfile) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("idontexist@test.com", "444444");
  EXPECT_FALSE(
      client.WallpaperControllerClientImpl::IsWallpaperSyncEnabled(account_id));
}

}  // namespace
