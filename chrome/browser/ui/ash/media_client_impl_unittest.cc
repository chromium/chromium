// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/media_controller.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "chrome/browser/ash/extensions/media_player_api.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/user_manager/fake_user_manager.h"
#include "media/capture/video/video_capture_device_info.h"
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

class FakeNotificationDisplayService : public NotificationDisplayService {
 public:
  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    show_called_times_++;
    active_notifications_.insert_or_assign(notification.id(), notification);
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    active_notifications_.erase(notification_id);
  }

  void GetDisplayed(DisplayedNotificationsCallback callback) override {}

  void AddObserver(NotificationDisplayService::Observer* observer) override {}
  void RemoveObserver(NotificationDisplayService::Observer* observer) override {
  }

  // Returns true if any existing notification contains `keywords` as a
  // substring.
  bool HasNotificationMessageContaining(const std::string& keywords) const {
    const std::u16string keywords_u16 = base::UTF8ToUTF16(keywords);
    for (const auto& [notification_id, notification] : active_notifications_) {
      if (notification.message().find(keywords_u16) != std::u16string::npos) {
        return true;
      }
    }
    return false;
  }

  size_t NumberOfActiveNotifications() const {
    return active_notifications_.size();
  }

  size_t show_called_times() const { return show_called_times_; }

  void SimulateClick(const std::string& id, absl::optional<int> button_idx) {
    auto notification_iter = active_notifications_.find(id);
    ASSERT_TRUE(notification_iter != active_notifications_.end());

    message_center::Notification notification = notification_iter->second;

    notification.delegate()->Click(button_idx, absl::nullopt);

    if (notification.rich_notification_data().remove_on_click) {
      active_notifications_.erase(id);
    }
  }

 private:
  std::map<std::string, message_center::Notification> active_notifications_;
  size_t show_called_times_ = 0;
};

class MockNewWindowDelegate
    : public testing::NiceMock<ash::TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
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

  static apps::CapabilityAccessPtr MakeCapabilityAccess(
      const char* app_id,
      absl::optional<bool> camera) {
    apps::CapabilityAccessPtr access =
        std::make_unique<apps::CapabilityAccess>(app_id);
    access->camera = camera;
    access->microphone = false;
    return access;
  }

  void LaunchApp(const char* id,
                 const char* name,
                 absl::optional<bool> use_camera) {
    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name));
    registry_cache_.OnApps(std::move(registry_deltas), apps::AppType::kUnknown,
                           /* should_notify_initialized = */ false);

    std::vector<apps::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(MakeCapabilityAccess(id, use_camera));
    capability_access_cache_.OnCapabilityAccesses(
        std::move(capability_access_deltas));
  }

  const std::string kPrimaryProfileName = "primary_profile";
  const AccountId account_id_ = AccountId::FromUserEmail(kPrimaryProfileName);

  apps::AppRegistryCache registry_cache_;
  apps::AppCapabilityAccessCache capability_access_cache_;
};

class MediaClientAppUsingCameraInBrowserEnvironmentTest
    : public MediaClientAppUsingCameraTest {
 public:
  MediaClientAppUsingCameraInBrowserEnvironmentTest() {
    user_manager_.Initialize();
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    window_delegate_provider_ =
        std::make_unique<ash::TestNewWindowDelegateProvider>(
            std::move(delegate));
  }

  ~MediaClientAppUsingCameraInBrowserEnvironmentTest() override {
    user_manager_.Shutdown();
    user_manager_.Destroy();
  }

  void LaunchAppUpdateActiveClientCount(const char* id,
                                        const char* name,
                                        absl::optional<bool> use_camera,
                                        int active_client_count) {
    media_client_.active_camera_client_count_ = active_client_count;
    LaunchApp(id, name, use_camera);
  }

  void SetCameraHWPrivacySwitchState(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state) {
    media_client_.device_id_to_camera_privacy_switch_state_[device_id] = state;
  }

  // Adds the device with id `device_id` to the map of active devices. To
  // display hardware switch notifications associated to this device, the device
  // needs to be active.
  void MakeDeviceActive(const std::string& device_id) {
    media_client_
        .devices_used_by_client_[cros::mojom::CameraClientType::CHROME] = {
        device_id};
  }

  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      const base::flat_set<std::string>& active_device_ids,
      int active_client_count) {
    media_client_.devices_used_by_client_.insert_or_assign(type,
                                                           active_device_ids);
    media_client_.active_camera_client_count_ = active_client_count;

    media_client_.OnGetSourceInfosByActiveClientChanged(active_device_ids,
                                                        video_capture_devices_);
  }

  void AttachCamera(const std::string& device_id,
                    const std::string& device_name) {
    media::VideoCaptureDeviceInfo device_info;
    device_info.descriptor.device_id = device_id;
    device_info.descriptor.set_display_name(device_name);
    video_capture_devices_.push_back(device_info);
  }

  // Detaches the most recently attached camera.
  void DetachCamera() { video_capture_devices_.pop_back(); }

  void ShowCameraOffNotification(const std::string& device_id,
                                 const std::string& device_name) {
    media_client_.ShowCameraOffNotification(device_id, device_name);
  }

  void OnCapabilityAccessUpdate(
      const apps::CapabilityAccessUpdate& capability_update) {
    media_client_.OnCapabilityAccessUpdate(capability_update);
  }

  apps::CapabilityAccessUpdate MakeCapabilityAccessUpdate(
      const apps::CapabilityAccess* capability) const {
    return apps::CapabilityAccessUpdate(capability, nullptr, account_id_);
  }

  FakeNotificationDisplayService* SetSystemNotificationService() const {
    std::unique_ptr<FakeNotificationDisplayService>
        fake_notification_display_service =
            std::make_unique<FakeNotificationDisplayService>();
    FakeNotificationDisplayService* fake_notification_display_service_ptr =
        fake_notification_display_service.get();
    SystemNotificationHelper::GetInstance()->SetSystemServiceForTesting(
        std::move(fake_notification_display_service));

    return fake_notification_display_service_ptr;
  }

 protected:
  // Has to be the first member as others are CHECKing the environment in their
  // constructors.
  content::BrowserTaskEnvironment task_environment_;

  MediaClientImpl media_client_;
  SystemNotificationHelper system_notification_helper_;
  user_manager::FakeUserManager user_manager_;
  raw_ptr<MockNewWindowDelegate, DanglingUntriaged> new_window_delegate_ =
      nullptr;
  std::unique_ptr<ash::TestNewWindowDelegateProvider> window_delegate_provider_;
  std::vector<media::VideoCaptureDeviceInfo> video_capture_devices_;
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
  std::string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  EXPECT_TRUE(app_name.empty());
}

TEST_F(MediaClientAppUsingCameraTest, AppLaunchedNotUsingCamaera) {
  LaunchApp("id_rose", "name_rose", /*use_camera=*/false);

  // Should return an empty string.
  std::string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  EXPECT_TRUE(app_name.empty());
}

TEST_F(MediaClientAppUsingCameraTest, AppLaunchedUsingCamera) {
  LaunchApp("id_rose", "name_rose", /*use_camera=*/true);

  // Should return the name of our app.
  std::string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  EXPECT_STREQ(app_name.c_str(), "name_rose");
}

TEST_F(MediaClientAppUsingCameraTest, MultipleAppsLaunchedUsingCamera) {
  LaunchApp("id_rose", "name_rose", /*use_camera=*/true);
  LaunchApp("id_mars", "name_mars", /*use_camera=*/true);
  LaunchApp("id_zara", "name_zara", /*use_camera=*/true);
  LaunchApp("id_oscar", "name_oscar", /*use_camera=*/false);

  // Because AppCapabilityAccessCache::GetAppsAccessingCamera (invoked by
  // GetNameOfAppAccessingCamera) returns a set, we have no guarantee of
  // which app will be found first.  So we verify that the app name is one of
  // our camera-users.
  std::string app_name = MediaClientImpl::GetNameOfAppAccessingCamera(
      &capability_access_cache_, &registry_cache_);
  EXPECT_THAT(app_name, AnyOf("name_rose", "name_mars", "name_zara"));
}

TEST_F(MediaClientAppUsingCameraInBrowserEnvironmentTest,
       OnCapabilityAccessUpdate) {
  const FakeNotificationDisplayService* notification_display_service =
      SetSystemNotificationService();
  const char* app1_id = "app1";
  const char* app2_id = "app2";
  const char* app1_name = "App name";
  const char* app2_name = "Other app";
  const apps::CapabilityAccessPtr capability_access =
      MakeCapabilityAccess(app1_id, false);
  const apps::CapabilityAccessUpdate capability_access_update =
      MakeCapabilityAccessUpdate(capability_access.get());
  const char* generic_notification_message_prefix =
      "An app is trying to access";

  user_manager_.AddUser(account_id_);
  ASSERT_TRUE(user_manager::UserManager::Get()->GetActiveUser());

  EXPECT_EQ(notification_display_service->show_called_times(), 0u);

  // No apps are active.
  OnCapabilityAccessUpdate(capability_access_update);
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 0u);

  // Launch an app. The notification shouldn't be active yet.
  LaunchAppUpdateActiveClientCount(app1_id, app1_name, true, 1);
  EXPECT_EQ(notification_display_service->show_called_times(), 0u);
  // As there is no state change of camera usage by the app the notification
  // shouldn't be shown either.
  OnCapabilityAccessUpdate(capability_access_update);
  EXPECT_EQ(notification_display_service->show_called_times(), 0u);

  // Showing the camera notification, e.g. because the privacy switch was
  // toggled.
  SetCameraHWPrivacySwitchState("device_id",
                                cros::mojom::CameraPrivacySwitchState::ON);
  MakeDeviceActive("device_id");
  ShowCameraOffNotification("device_id", "device_name");
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 1u);
  EXPECT_TRUE(notification_display_service->HasNotificationMessageContaining(
      generic_notification_message_prefix));
  EXPECT_EQ(notification_display_service->show_called_times(), 1u);

  // Start a second app that's also using the camera.
  LaunchApp(app2_id, app2_name, true);
  EXPECT_TRUE(notification_display_service->HasNotificationMessageContaining(
      generic_notification_message_prefix));
  EXPECT_EQ(notification_display_service->show_called_times(), 1u);

  // Launching an App with `use_camera=false` is like minimizing/closing the
  // app for the purpose of this test.
  LaunchApp(app1_id, app1_name, false);

  OnCapabilityAccessUpdate(capability_access_update);

  // After the observer reacted to the change the notification should not pop up
  // again but update the message body if necessary (which it isn't currently).
  EXPECT_EQ(notification_display_service->show_called_times(), 2u);
  EXPECT_TRUE(notification_display_service->HasNotificationMessageContaining(
      generic_notification_message_prefix));
  ASSERT_EQ(notification_display_service->NumberOfActiveNotifications(), 1u);
}

TEST_F(MediaClientAppUsingCameraInBrowserEnvironmentTest,
       NotificationRemovedWhenSWSwitchChangedToON) {
  const FakeNotificationDisplayService* notification_display_service =
      SetSystemNotificationService();
  const char* app_id = "app_id";
  const char* app_name = "app_name";
  const apps::CapabilityAccessPtr capability_access =
      MakeCapabilityAccess(app_id, false);
  const apps::CapabilityAccessUpdate capability_access_update =
      MakeCapabilityAccessUpdate(capability_access.get());

  user_manager_.AddUser(account_id_);
  ASSERT_TRUE(user_manager::UserManager::Get()->GetActiveUser());

  // No apps are active.
  OnCapabilityAccessUpdate(capability_access_update);
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 0u);

  // Launch an app. The notification shouldn't be displayed yet.
  LaunchAppUpdateActiveClientCount(app_id, app_name, true, 1);
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 0u);

  // Showing the camera notification, e.g. because the hardware privacy switch
  // was toggled.
  SetCameraHWPrivacySwitchState("device_id",
                                cros::mojom::CameraPrivacySwitchState::ON);
  MakeDeviceActive("device_id");
  ShowCameraOffNotification("device_id", "device_name");
  // One notification should be displayed.
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 1u);

  // Setting the software privacy switch to ON. The existing hardware switch
  // notification should be removed.
  media_client_.OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 0u);
}

TEST_F(MediaClientAppUsingCameraInBrowserEnvironmentTest,
       LearnMoreButtonInteraction) {
  FakeNotificationDisplayService* notification_display_service =
      SetSystemNotificationService();
  const char* app_id = "app";
  const char* app_name = "App name";
  const apps::CapabilityAccessPtr capability_access =
      MakeCapabilityAccess(app_id, false);

  user_manager_.AddUser(account_id_);
  ASSERT_TRUE(user_manager::UserManager::Get()->GetActiveUser());

  EXPECT_EQ(notification_display_service->show_called_times(), 0u);

  LaunchAppUpdateActiveClientCount(app_id, app_name, true, 1);

  // Showing the camera notification, e.g. because the privacy switch was
  // toggled.
  SetCameraHWPrivacySwitchState("device_id",
                                cros::mojom::CameraPrivacySwitchState::ON);
  MakeDeviceActive("device_id");
  ShowCameraOffNotification("device_id", "device_name");

  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 1u);
  EXPECT_CALL(*new_window_delegate_, OpenUrl).Times(1);

  notification_display_service->SimulateClick(
      "ash.media.camera.activity_with_privacy_switch_on.device_id", 0);

  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 0u);
}

TEST_F(MediaClientAppUsingCameraInBrowserEnvironmentTest,
       NotificationRemovedWhenCameraDetachedOrInactive) {
  FakeNotificationDisplayService* notification_display_service =
      SetSystemNotificationService();

  // No notification initially.
  EXPECT_EQ(0u, notification_display_service->NumberOfActiveNotifications());

  const std::string camera1 = "camera1";
  const std::string camera1_name = "Fake camera 1";
  const std::string camera2 = "camera2";
  const std::string camera2_name = "Fake camera 2";

  // Attach two cameras to the device. Both of the cameras have HW switch. Turn
  // the HW switch ON for both of the devices.
  AttachCamera(camera1, camera1_name);
  SetCameraHWPrivacySwitchState(camera1,
                                cros::mojom::CameraPrivacySwitchState::ON);
  AttachCamera(camera2, camera2_name);
  SetCameraHWPrivacySwitchState(camera2,
                                cros::mojom::CameraPrivacySwitchState::ON);

  // Still no notification.
  EXPECT_EQ(notification_display_service->NumberOfActiveNotifications(), 0u);

  // `CHROME` client starts accessing camera1. A hardware switch notification
  // for camera1 should be displayed.
  OnActiveClientChange(cros::mojom::CameraClientType::CHROME, {camera1}, 1);
  EXPECT_EQ(1u, notification_display_service->NumberOfActiveNotifications());
  EXPECT_TRUE(notification_display_service->HasNotificationMessageContaining(
      camera1_name));

  // `CHROME` client starts accessing camera2 as well. A hardware switch
  // notification for camera2 should be displayed.
  OnActiveClientChange(cros::mojom::CameraClientType::CHROME,
                       {camera1, camera2}, 1);
  EXPECT_EQ(2u, notification_display_service->NumberOfActiveNotifications());
  EXPECT_TRUE(notification_display_service->HasNotificationMessageContaining(
      camera2_name));

  // `CHROME` client stops accessing camera1. The respective notification should
  // be removed.
  OnActiveClientChange(cros::mojom::CameraClientType::CHROME, {camera2}, 1);
  EXPECT_EQ(1u, notification_display_service->NumberOfActiveNotifications());
  EXPECT_FALSE(notification_display_service->HasNotificationMessageContaining(
      camera1_name));

  // Detach camera2.
  DetachCamera();
  // `CHROME` client stops accessing camera2 as the camera is detached. The
  // respective notification should be removed.
  OnActiveClientChange(cros::mojom::CameraClientType::CHROME, {}, 0);
  EXPECT_EQ(0u, notification_display_service->NumberOfActiveNotifications());
}
