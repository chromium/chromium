// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/browser/value_store/testing_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kChromeAppDailyRefreshInfoKey[] = "daily-refresh-info-key";

class WallpaperControllerClientImplTest : public testing::Test {
 public:
  WallpaperControllerClientImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~WallpaperControllerClientImplTest() override = default;

 private:
  ScopedTestingLocalState local_state_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerClientImplTest);
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

  TestingValueStore value_store;

  // There is also a resume token and an enabled state, but that's not what is
  // being tested here, so only populate collectionId.
  std::string json("{\"collectionId\" : \"fun_collection\"}");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  client.MigrateCollectionIdFromValueStoreForTesting(&value_store);

  EXPECT_EQ("fun_collection", controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNoCollectionId) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  TestingValueStore value_store;

  // There is also a resume token and an enabled state, but that's not what is
  // being tested here, so only populate collectionId.
  std::string json("{\"collectionId\" : null}");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  client.MigrateCollectionIdFromValueStoreForTesting(&value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNoStore) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  client.MigrateCollectionIdFromValueStoreForTesting(nullptr);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNotOKStatusCode) {
  using StatusCode = ValueStore::StatusCode;

  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  TestingValueStore value_store;
  value_store.set_status_code(StatusCode::OTHER_ERROR);

  // There is also a resume token and an enabled state, but that's not what is
  // being tested here, so only populate collectionId.
  std::string json("{\"collectionId\" : \"fun_collection\"}");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  client.MigrateCollectionIdFromValueStoreForTesting(&value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreNoPref) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  TestingValueStore value_store;
  client.MigrateCollectionIdFromValueStoreForTesting(&value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

TEST_F(WallpaperControllerClientImplTest,
       MigrateCollectionIdFromValueStoreMalformedPref) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  TestingValueStore value_store;
  std::string json("{");
  value_store.Set(0, kChromeAppDailyRefreshInfoKey, base::Value(json));
  client.MigrateCollectionIdFromValueStoreForTesting(&value_store);

  EXPECT_EQ(std::string(), controller.collection_id());
}

}  // namespace
