// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_client_impl.h"

#include <memory>

#include "ash/public/cpp/media_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/media_keys_listener.h"

// Gmock matchers and actions that are used below.
using ::testing::AnyOf;

namespace {

class TestMediaController : public ash::MediaController {
 public:
  TestMediaController() = default;

  TestMediaController(const TestMediaController&) = delete;
  TestMediaController& operator=(const TestMediaController&) = delete;

  ~TestMediaController() override = default;

  // ash::MediaController:
  void SetClient(ash::MediaClient* client) override {}
  void SetForceMediaClientKeyHandling(bool enabled) override {
    force_media_client_key_handling_ = enabled;
  }
  void NotifyCaptureState(
      const base::flat_map<AccountId, ash::MediaCaptureState>& capture_states)
      override {}

  void NotifyVmMediaNotificationState(bool camera,
                                      bool mic,
                                      bool camera_and_mic) override {}

  bool force_media_client_key_handling() const {
    return force_media_client_key_handling_;
  }

 private:
  bool force_media_client_key_handling_ = false;
};

class TestMediaKeysDelegate : public ui::MediaKeysListener::Delegate {
 public:
  TestMediaKeysDelegate() = default;

  TestMediaKeysDelegate(const TestMediaKeysDelegate&) = delete;
  TestMediaKeysDelegate& operator=(const TestMediaKeysDelegate&) = delete;

  ~TestMediaKeysDelegate() override = default;

  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override {
    last_media_key_ = accelerator;
  }

  absl::optional<ui::Accelerator> ConsumeLastMediaKey() {
    absl::optional<ui::Accelerator> key = last_media_key_;
    last_media_key_.reset();
    return key;
  }

 private:
  absl::optional<ui::Accelerator> last_media_key_;
};

}  // namespace

class MediaClientTest : public BrowserWithTestWindowTest {
 public:
  MediaClientTest() = default;

  MediaClientTest(const MediaClientTest&) = delete;
  MediaClientTest& operator=(const MediaClientTest&) = delete;

  ~MediaClientTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    alt_window_ = CreateBrowserWindow();
    alt_browser_ = CreateBrowser(alt_profile(), Browser::TYPE_NORMAL, false,
                                 alt_window_.get());

    extensions::MediaPlayerAPI::Get(profile());

    test_delegate_ = std::make_unique<TestMediaKeysDelegate>();

    media_controller_resetter_ =
        std::make_unique<ash::MediaController::ScopedResetterForTest>();
    test_media_controller_ = std::make_unique<TestMediaController>();

    media_client_ = std::make_unique<MediaClientImpl>();
    media_client_->InitForTesting(test_media_controller_.get());

    BrowserList::SetLastActive(browser());

    ASSERT_FALSE(test_media_controller_->force_media_client_key_handling());
    ASSERT_EQ(absl::nullopt, delegate()->ConsumeLastMediaKey());
  }

  void TearDown() override {
    media_client_.reset();
    test_media_controller_.reset();
    media_controller_resetter_.reset();
    test_delegate_.reset();

    alt_browser_->tab_strip_model()->CloseAllTabs();
    alt_browser_.reset();
    alt_window_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  MediaClientImpl* client() { return media_client_.get(); }

  TestMediaController* controller() { return test_media_controller_.get(); }

  Profile* alt_profile() {
    return profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  Browser* alt_browser() { return alt_browser_.get(); }

  TestMediaKeysDelegate* delegate() { return test_delegate_.get(); }

 private:
  std::unique_ptr<TestMediaKeysDelegate> test_delegate_;
  std::unique_ptr<ash::MediaController::ScopedResetterForTest>
      media_controller_resetter_;
  std::unique_ptr<TestMediaController> test_media_controller_;
  std::unique_ptr<MediaClientImpl> media_client_;

  std::unique_ptr<Browser> alt_browser_;
  std::unique_ptr<BrowserWindow> alt_window_;
};

class MediaClientAppUsingCameraTest : public testing::Test {
 public:
  MediaClientAppUsingCameraTest() = default;
  MediaClientAppUsingCameraTest(const MediaClientAppUsingCameraTest&) = delete;
  MediaClientAppUsingCameraTest& operator=(
      const MediaClientAppUsingCameraTest&) = delete;
  ~MediaClientAppUsingCameraTest() override = default;

  void SetUp() override {
    registry_cache_.SetAccountId(account_id_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             &registry_cache_);
    capability_access_cache_.SetAccountId(account_id_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_, &capability_access_cache_);
  }

 protected:
  static apps::AppPtr MakeApp(const char* app_id, const char* name) {
    apps::AppPtr app =
        std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
    app->name = name;
    app->short_name = name;
    return app;
  }

  static apps::mojom::CapabilityAccessPtr MakeCapabilityAccess(
      const char* app_id,
      apps::mojom::OptionalBool camera) {
    apps::mojom::CapabilityAccessPtr access =
        apps::mojom::CapabilityAccess::New();
    access->app_id = app_id;
    access->camera = camera;
    access->microphone = apps::mojom::OptionalBool::kFalse;
    return access;
  }

  void LaunchApp(const char* id,
                 const char* name,
                 apps::mojom::OptionalBool use_camera) {
    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name));
    if (base::FeatureList::IsEnabled(
            apps::kAppServiceOnAppUpdateWithoutMojom)) {
      registry_cache_.OnApps(std::move(registry_deltas),
                             apps::AppType::kUnknown,
                             /* should_notify_initialized = */ false);
    } else {
      std::vector<apps::mojom::AppPtr> mojom_deltas;
      mojom_deltas.push_back(apps::ConvertAppToMojomApp(registry_deltas[0]));
      registry_cache_.OnApps(std::move(mojom_deltas),
                             apps::mojom::AppType::kUnknown,
                             /* should_notify_initialized = */ false);
    }

    std::vector<apps::mojom::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(MakeCapabilityAccess(id, use_camera));
    capability_access_cache_.OnCapabilityAccesses(
        std::move(capability_access_deltas));
  }

  const std::string kPrimaryProfileName = "primary_profile";
  const AccountId account_id_ = AccountId::FromUserEmail(kPrimaryProfileName);

  apps::AppRegistryCache registry_cache_;
  apps::AppCapabilityAccessCache capability_access_cache_;
};

TEST_F(MediaClientTest, HandleMediaAccelerators) {
  const struct {
    ui::Accelerator accelerator;
    base::RepeatingClosure client_handler;
  } kTestCases[] = {
      {ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPlayPause,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_PLAY, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPlay,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_PAUSE, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPause,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_STOP, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaStop,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaNextTrack,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPrevTrack,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_OEM_103, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaSeekBackward,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_OEM_104, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaSeekForward,
                           base::Unretained(client()))}};

  for (auto& test : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "accelerator key:" << test.accelerator.key_code());

    // Enable custom media key handling for the current browser. Ensure that
    // the client set the override on the controller.
    client()->EnableCustomMediaKeyHandler(profile(), delegate());
    EXPECT_TRUE(controller()->force_media_client_key_handling());

    // Simulate the media key and check that the delegate received it.
    test.client_handler.Run();
    EXPECT_EQ(test.accelerator, delegate()->ConsumeLastMediaKey());

    // Change the active browser and ensure the override was disabled.
    BrowserList::SetLastActive(alt_browser());
    EXPECT_FALSE(controller()->force_media_client_key_handling());

    // Simulate the media key and check that the delegate did not receive it.
    test.client_handler.Run();
    EXPECT_EQ(absl::nullopt, delegate()->ConsumeLastMediaKey());

    // Change the active browser back and ensure the override was enabled.
    BrowserList::SetLastActive(browser());
    EXPECT_TRUE(controller()->force_media_client_key_handling());

    // Simulate the media key and check the delegate received it.
    test.client_handler.Run();
    EXPECT_EQ(test.accelerator, delegate()->ConsumeLastMediaKey());

    // Disable custom media key handling for the current browser and ensure
    // the override was disabled.
    client()->DisableCustomMediaKeyHandler(profile(), delegate());
    EXPECT_FALSE(controller()->force_media_client_key_handling());

    // Simulate the media key and check the delegate did not receive it.
    test.client_handler.Run();
    EXPECT_EQ(absl::nullopt, delegate()->ConsumeLastMediaKey());
  }
}

TEST_F(MediaClientAppUsingCameraTest, NoAppsLaunched) {
  // Should return an empty string.
  std::u16string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  EXPECT_TRUE(app_name.empty());
}

TEST_F(MediaClientAppUsingCameraTest, AppLaunchedNotUsingCamaera) {
  LaunchApp("id_rose", "name_rose", apps::mojom::OptionalBool::kFalse);

  // Should return an empty string.
  std::u16string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  EXPECT_TRUE(app_name.empty());
}

TEST_F(MediaClientAppUsingCameraTest, AppLaunchedUsingCamera) {
  LaunchApp("id_rose", "name_rose", apps::mojom::OptionalBool::kTrue);

  // Should return the name of our app.
  std::u16string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  std::string app_name_utf8 = base::UTF16ToUTF8(app_name);
  EXPECT_STREQ(app_name_utf8.c_str(), "name_rose");
}

TEST_F(MediaClientAppUsingCameraTest, MultipleAppsLaunchedUsingCamera) {
  LaunchApp("id_rose", "name_rose", apps::mojom::OptionalBool::kTrue);
  LaunchApp("id_mars", "name_mars", apps::mojom::OptionalBool::kTrue);
  LaunchApp("id_zara", "name_zara", apps::mojom::OptionalBool::kTrue);
  LaunchApp("id_oscar", "name_oscar", apps::mojom::OptionalBool::kFalse);

  // Because AppCapabilityAccessCache::GetAppsAccessingCamera (invoked by
  // GetNameOfAppAccessingCamera) returns a set, we have no guarantee of
  // which app will be found first.  So we verify that the app name is one of
  // our camera-users.
  std::u16string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  std::string app_name_utf8 = base::UTF16ToUTF8(app_name);
  EXPECT_THAT(app_name_utf8, AnyOf("name_rose", "name_mars", "name_zara"));
}
