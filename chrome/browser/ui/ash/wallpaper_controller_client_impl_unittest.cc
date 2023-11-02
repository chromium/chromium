// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
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

class WallpaperControllerClientImplTest : public testing::Test {
 public:
  WallpaperControllerClientImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void SetUp() override {
    testing::Test::SetUp();
    client_.InitForTesting(&controller_);
  }

  TestWallpaperController* controller() { return &controller_; }
  WallpaperControllerClientImpl* client() { return &client_; }

  WallpaperControllerClientImplTest(const WallpaperControllerClientImplTest&) =
      delete;
  WallpaperControllerClientImplTest& operator=(
      const WallpaperControllerClientImplTest&) = delete;

  ~WallpaperControllerClientImplTest() override = default;

  void OnGooglePhotosDailyAlbumFetched(
      const AccountId& account_id,
      WallpaperControllerClientImpl::FetchGooglePhotosPhotoCallback callback,
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
          response) {
    client_.OnGooglePhotosDailyAlbumFetched(account_id, std::move(callback),
                                            std::move(response));
  }

 private:
  ScopedTestingLocalState local_state_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
  TestWallpaperController controller_;
  WallpaperControllerClientImpl client_;
};

TEST_F(WallpaperControllerClientImplTest, Construction) {
  // Singleton was initialized.
  EXPECT_EQ(client(), WallpaperControllerClientImpl::Get());

  // Object was set as client.
  EXPECT_TRUE(controller()->was_client_set());
}

TEST_F(WallpaperControllerClientImplTest, IsWallpaperSyncEnabledNoProfile) {
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("idontexist@test.com", "444444");
  EXPECT_FALSE(client()->WallpaperControllerClientImpl::IsWallpaperSyncEnabled(
      account_id));
}

TEST_F(WallpaperControllerClientImplTest, DailyGooglePhotosDoNotRepeat) {
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("idontexist@test.com", "444444");

  // Size big enough to have not hit the minimum cache size logic, but small
  // enough to make sure we're very unlikely to pass if the logic is broken,
  // since there's randomness involved here.
  const int photos_in_album = 20;

  // Create a fake response with fake photos (the id is the only parameter that
  // matters here).
  auto response =
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New(
          std::vector<GooglePhotosPhotoPtr>(), absl::nullopt);
  for (int i = 0; i < photos_in_album; i++) {
    response->photos->push_back(
        ash::personalization_app::mojom::GooglePhotosPhoto::New(
            "id" + base::NumberToString(i), /*dedup_key=*/absl::nullopt,
            /*name=*/"", /*date=*/u"",
            /*url=*/GURL(""), /*location=*/absl::nullopt));
  }

  std::deque<std::string> last_ten;
  auto handle_photo = [&last_ten](GooglePhotosPhotoPtr photo, bool success) {
    ASSERT_TRUE(success);

    EXPECT_FALSE(base::Contains(last_ten, photo->id));

    last_ten.push_back(photo->id);

    if (last_ten.size() > 10)
      last_ten.pop_front();
  };

  for (int i = 0; i < 20; i++) {
    OnGooglePhotosDailyAlbumFetched(account_id,
                                    base::BindLambdaForTesting(handle_photo),
                                    response->Clone());
  }
}

}  // namespace
