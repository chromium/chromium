// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/media_notification_provider_impl.h"
#include <memory>

#include "ash/system/media/media_notification_provider_observer.h"
#include "ash/test_shell_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_environment.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "components/media_message_center/notification_theme.h"
#include "components/media_router/common/media_route.h"
#include "content/public/test/browser_task_environment.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

using global_media_controls::mojom::DeviceListClient;
using global_media_controls::mojom::DeviceListHost;
using global_media_controls::mojom::DevicePickerProvider;
using global_media_controls::test::MockDeviceService;
using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

namespace ash {

namespace {

class MockCastMediaNotificationItem : public CastMediaNotificationItem {
 public:
  MockCastMediaNotificationItem(
      const media_router::MediaRoute& route,
      global_media_controls::MediaItemManager* item_manager,
      Profile* profile)
      : CastMediaNotificationItem(route, item_manager, nullptr, profile) {}

  MOCK_METHOD(void, StopCasting, ());
};

class MockMediaNotificationProviderObserver
    : public MediaNotificationProviderObserver {
 public:
  MOCK_METHOD(void, OnNotificationListChanged, ());
  MOCK_METHOD(void, OnNotificationListViewSizeChanged, ());
};

class FakeMediaSessionService : public media_session::MediaSessionService {
 public:
  FakeMediaSessionService() = default;
  FakeMediaSessionService(const FakeMediaSessionService&) = delete;
  FakeMediaSessionService& operator=(const FakeMediaSessionService&) = delete;
  ~FakeMediaSessionService() override = default;

  // media_session::MediaSessionService:
  void BindAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override {}
  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override {}
  void BindMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override {}
};

class MediaTestShellDelegate : public TestShellDelegate {
 public:
  MediaTestShellDelegate() = default;
  ~MediaTestShellDelegate() override = default;

  // ShellDelegate:
  media_session::MediaSessionService* GetMediaSessionService() override {
    return &media_session_service_;
  }
  std::unique_ptr<MediaNotificationProvider> CreateMediaNotificationProvider()
      override {
    return std::make_unique<MediaNotificationProviderImpl>(
        GetMediaSessionService());
  }

 private:
  FakeMediaSessionService media_session_service_;
};

class TestMediaNotificationItem
    : public media_message_center::test::MockMediaNotificationItem {
 public:
  std::optional<base::UnguessableToken> GetSourceId() const override {
    return source_id_;
  }

 private:
  const base::UnguessableToken source_id_{base::UnguessableToken::Create()};
};

}  // namespace

class MediaNotificationProviderImplTest : public ChromeAshTestBase {
 protected:
  MediaNotificationProviderImplTest()
      : ChromeAshTestBase(std::make_unique<content::BrowserTaskEnvironment>(
            content::BrowserTaskEnvironment::REAL_IO_THREAD)) {}
  ~MediaNotificationProviderImplTest() override = default;

  void SetUp() override {
    auto shell_delegate = std::make_unique<MediaTestShellDelegate>();
    shell_delegate_ = shell_delegate.get();
    ChromeAshTestBase::SetUp(std::move(shell_delegate));

    crosapi_environment_.SetUp();
    provider_ = static_cast<MediaNotificationProviderImpl*>(
        MediaNotificationProvider::Get());
    provider_->SetColorTheme(media_message_center::NotificationTheme());
    observer_ = std::make_unique<MockMediaNotificationProviderObserver>();
    provider_->AddObserver(observer_.get());
    layout_provider_ = std::make_unique<ChromeLayoutProvider>();
  }

  void TearDown() override {
    observer_.reset();
    crosapi_environment_.TearDown();
    AshTestBase::TearDown();
  }

  void SimulateShowNotification(base::UnguessableToken id) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);

    provider_->media_session_item_producer_for_testing()->OnFocusGained(
        std::move(focus));
    provider_->media_session_item_producer_for_testing()->ActivateItem(
        id.ToString());
  }

  void SimulateHideNotification(base::UnguessableToken id) {
    provider_->media_session_item_producer_for_testing()->HideItem(
        id.ToString());
  }

  void SimulateRefreshNotification(base::UnguessableToken id) {
    provider_->media_session_item_producer_for_testing()->RefreshItem(
        id.ToString());
  }

  std::unique_ptr<global_media_controls::MediaItemUIListView>
  CreateNotificationListView() {
    auto view = provider_->GetMediaNotificationListView(
        1, /*should_clip_height=*/true,
        global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray,
        /*show_devices_for_item_id=*/"");
    return base::WrapUnique(
        static_cast<global_media_controls::MediaItemUIListView*>(
            view.release()));
  }

  std::unique_ptr<ChromeLayoutProvider> layout_provider_;
  std::unique_ptr<MockMediaNotificationProviderObserver> observer_;
  raw_ptr<MediaNotificationProviderImpl, DanglingUntriaged> provider_ = nullptr;
  raw_ptr<MediaTestShellDelegate, DanglingUntriaged> shell_delegate_ = nullptr;
  crosapi::TestCrosapiEnvironment crosapi_environment_;
};

TEST_F(MediaNotificationProviderImplTest, NotificationListTest) {
  auto id_1 = base::UnguessableToken::Create();
  auto id_2 = base::UnguessableToken::Create();

  EXPECT_CALL(*observer_, OnNotificationListViewSizeChanged).Times(2);
  SimulateShowNotification(id_1);
  SimulateShowNotification(id_2);
  auto notification_list_view = CreateNotificationListView();
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 2u);

  EXPECT_CALL(*observer_, OnNotificationListViewSizeChanged);
  SimulateHideNotification(id_1);
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 1u);

  provider_->OnBubbleClosing();
}

TEST_F(MediaNotificationProviderImplTest, NotifyObserverOnListChangeTest) {
  auto id = base::UnguessableToken::Create();

  // Expecting 2 calls: one when MediaSessionNotificationItem is created in
  // MediaNotificationService::OnFocusgained, one when
  // MediaNotificationService::ShowNotification is called in
  // SimulateShownotification.
  EXPECT_CALL(*observer_, OnNotificationListChanged).Times(2);
  SimulateShowNotification(id);

  EXPECT_CALL(*observer_, OnNotificationListChanged);
  SimulateHideNotification(id);
}

// Regression test for https://crbug.com/1312419. This should not crash on ASan
// builds (or any other build of course).
TEST_F(MediaNotificationProviderImplTest, DontUseDeletedListView) {
  // Simulate a media session item.
  auto id = base::UnguessableToken::Create();
  SimulateShowNotification(id);

  // Create a list view with that item.
  auto notification_list_view = CreateNotificationListView();

  // Delete the list view.
  notification_list_view.reset();

  // Hide the item. This should not call into the deleted view.
  SimulateHideNotification(id);
}

TEST_F(MediaNotificationProviderImplTest, RefreshMediaItem) {
  auto id = base::UnguessableToken::Create();
  SimulateShowNotification(id);
  auto notification_list_view = CreateNotificationListView();

  EXPECT_EQ(notification_list_view->items_for_testing().size(), 1u);

  EXPECT_CALL(*observer_, OnNotificationListViewSizeChanged);
  SimulateRefreshNotification(id);
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 1u);
}

// Tests the `kGlobalMediaControlsCastStartStop` feature.
// TODO(crbug.com/1407071): Merge this test class into
// MediaNotificationProviderImplTest once the feature is enabled by default on
// Chrome OS.
class CastStartStopMediaNotificationProviderImplTest
    : public MediaNotificationProviderImplTest {
 public:
  void SetUp() override {
    // This must be called before MediaNotificationProviderImplTest::SetUp()
    // starts the GPU service thread.
    scoped_feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);
    MediaNotificationProviderImplTest::SetUp();

    profile_ = crosapi_environment_.profile_manager()->CreateTestingProfile(
        "Profile", /*is_main_profile=*/true);
    InitProvider();
  }

  void TearDown() override {
    profile_ = nullptr;
    // This is needed for avoiding a DCHECK failure caused by
    // TestNetworkConnectionTracker having an observer when it's destroyed.
    media_router::DnsSdRegistry::GetInstance()->ResetForTest();

    MediaNotificationProviderImplTest::TearDown();
  }

 protected:
  void InitProvider() {
    provider_->set_profile_for_testing(profile_);
    // We must initialize the list view before we can show individual media
    // items.
    list_view_ = provider_->GetMediaNotificationListView(
        1, /*should_clip_height=*/true,
        global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray,
        /*show_devices_for_item_id=*/"");
  }

  raw_ptr<Profile> profile_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::View> list_view_;
};

TEST_F(CastStartStopMediaNotificationProviderImplTest, ShowCastFooterView) {
  MockCastMediaNotificationItem item{
      media_router::MediaRoute{}, provider_->GetMediaItemManager(), profile_};
  auto* media_item_ui_view =
      provider_->ShowMediaItem("item_id", item.GetWeakPtr());
  global_media_controls::MediaItemUIFooter* footer_view =
      static_cast<global_media_controls::MediaItemUIView*>(media_item_ui_view)
          ->footer_view_for_testing();
  ASSERT_TRUE(footer_view);
  EXPECT_TRUE(footer_view && footer_view->GetVisible());

  // Click on the "Stop casting" button.
  EXPECT_CALL(item, StopCasting());
  views::Button* stop_casting_button =
      static_cast<views::Button*>(footer_view->children()[0]);
  views::test::ButtonTestApi(stop_casting_button)
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(0, 0), gfx::Point(0, 0),
          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
}

TEST_F(CastStartStopMediaNotificationProviderImplTest, ShowDeviceSelectorView) {
  MockDeviceService device_service;
  TestMediaNotificationItem item;
  provider_->set_device_service_for_testing(&device_service);
  auto* media_item_ui_view =
      provider_->ShowMediaItem("item_id", item.GetWeakPtr());
  global_media_controls::MediaItemUIDeviceSelector* selector_view =
      static_cast<global_media_controls::MediaItemUIView*>(media_item_ui_view)
          ->device_selector_view_for_testing();
  EXPECT_TRUE(selector_view);
}

TEST_F(CastStartStopMediaNotificationProviderImplTest,
       SetDevicePickerProvider) {
  provider_->OnPrimaryUserSessionStarted();

  MockDeviceService device_service;
  EXPECT_CALL(device_service, SetDevicePickerProvider);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->media_ui_ash()
      ->RegisterDeviceService(base::UnguessableToken::Create(),
                              device_service.PassRemote());
  device_service.FlushForTesting();
}

}  // namespace ash
