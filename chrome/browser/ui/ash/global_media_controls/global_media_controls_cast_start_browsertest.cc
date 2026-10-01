// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/global_media_controls/media_notification_provider_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/browser_test.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

class GlobalMediaControlsCastStartTest : public InProcessBrowserTest {
 public:
  GlobalMediaControlsCastStartTest() = default;
  ~GlobalMediaControlsCastStartTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlobalMediaControlsCastStartTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateMockMediaRouter(
      content::BrowserContext* context) {
    auto router =
        std::make_unique<testing::NiceMock<media_router::MockMediaRouter>>();
    ON_CALL(*router, RegisterMediaSinksObserver)
        .WillByDefault(testing::Return(true));
    return router;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &GlobalMediaControlsCastStartTest::CreateMockMediaRouter));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    media_router_ = static_cast<media_router::MockMediaRouter*>(
        media_router::MediaRouterFactory::GetApiForBrowserContext(
            GetProfile()));
  }

  void TearDownOnMainThread() override {
    media_router_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // `device_service_id` is the ID of the DeviceService resonsible for
  // binding the DeviceListHost that provides the list of devices.
  MediaItemUIDeviceSelectorView* ShowDevicePicker(
      MediaNotificationService* device_service,
      const base::UnguessableToken& device_service_id) {
    device_service->supplemental_device_picker_producer_
        ->GetOrCreateNotificationItem(device_service_id);
    device_service->supplemental_device_picker_producer_->ShowItem();

    MediaTray* media_tray = Shell::GetPrimaryRootWindowController()
                                ->shelf()
                                ->GetStatusAreaWidget()
                                ->media_tray();
    media_tray->ShowBubble();

    auto* list_view = static_cast<global_media_controls::MediaItemUIListView*>(
        media_tray->content_view_for_testing());
    auto* item_view = static_cast<global_media_controls::MediaItemUIView*>(
        list_view->contents()->children().at(0));
    return static_cast<MediaItemUIDeviceSelectorView*>(
        item_view->device_selector_view_for_testing());
  }

  // Returns the ID of the device that was added.
  std::string AddDevice(MediaItemUIDeviceSelectorView* device_picker) {
    const std::string device_id = "device123";
    global_media_controls::mojom::DevicePtr device =
        global_media_controls::mojom::Device::New();
    device->id = device_id;
    std::vector<global_media_controls::mojom::DevicePtr> devices;
    devices.push_back(std::move(device));
    device_picker->OnDevicesUpdated(std::move(devices));
    return device_id;
  }

  void SelectDevice(MediaItemUIDeviceSelectorView* selector_view,
                    const std::string& device_id) {
    const ui::MouseEvent pressed_event(
        ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
        ui::EF_LEFT_MOUSE_BUTTON);
    views::test::ButtonTestApi(
        selector_view->GetCastDeviceEntryViewsForTesting().at(0))
        .NotifyClick(pressed_event);
  }

  raw_ptr<media_router::MockMediaRouter> media_router_ = nullptr;
  base::CallbackListSubscription create_services_subscription_;
};

class GlobalMediaControlsCastStartTabMirroringTest
    : public GlobalMediaControlsCastStartTest {
 public:
  GlobalMediaControlsCastStartTabMirroringTest() {
    feature_list_.InitAndEnableFeature(
        media_router::kFallbackToAudioTabMirroring);
  }
  ~GlobalMediaControlsCastStartTabMirroringTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlobalMediaControlsCastStartRemotePlaybackTest
    : public GlobalMediaControlsCastStartTest {
 public:
  GlobalMediaControlsCastStartRemotePlaybackTest() {
    feature_list_.InitAndEnableFeature(media::kMediaRemotingWithoutFullscreen);
  }
  ~GlobalMediaControlsCastStartRemotePlaybackTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsCastStartTest, StartCasting) {
  auto* profile = GetProfile();
  auto* device_service =
      MediaNotificationServiceFactory::GetForProfile(profile);
  base::UnguessableToken device_service_id =
      content::MediaSession::GetSourceId(Profile::FromBrowserContext(profile));
  MediaItemUIDeviceSelectorView* device_picker =
      ShowDevicePicker(device_service, device_service_id);
  std::string device_id = AddDevice(device_picker);
  SelectDevice(device_picker, device_id);
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsCastStartTabMirroringTest,
                       StopCastingLocalMediaSessionTabMirroring) {
  content::WebContents* web_contents =
      browser()->GetActiveTabInterface()->GetContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  content::MediaSession::Get(web_contents);
  auto request_id =
      content::MediaSession::GetRequestIdFromWebContents(web_contents);

  auto* provider = static_cast<MediaNotificationProviderImpl*>(
      MediaNotificationProvider::Get());
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->is_controllable = true;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;

  media_session::mojom::AudioFocusRequestStatePtr focus(
      media_session::mojom::AudioFocusRequestState::New());
  focus->request_id = request_id;
  focus->session_info = std::move(session_info);

  provider->media_session_item_producer_for_testing()->OnFocusGained(
      std::move(focus));
  provider->media_session_item_producer_for_testing()->ActivateItem(
      request_id.ToString());

  const std::string route_id = "route_123";
  media_router::MediaRoute route(route_id,
                                 media_router::MediaSource::ForTab(tab_id),
                                 "sink_1", "Test Cast Route", true);
  route.set_media_sink_name("Living Room TV");
  EXPECT_CALL(*media_router_, GetCurrentRoutes())
      .WillRepeatedly(
          testing::Return(std::vector<media_router::MediaRoute>{route}));

  MediaTray* media_tray = Shell::GetPrimaryRootWindowController()
                              ->shelf()
                              ->GetStatusAreaWidget()
                              ->media_tray();
  media_tray->ShowBubble();

  auto* list_view = static_cast<global_media_controls::MediaItemUIListView*>(
      media_tray->content_view_for_testing());
  ASSERT_TRUE(list_view);
  ASSERT_EQ(1u, list_view->contents()->children().size());

  auto* item_view = static_cast<global_media_controls::MediaItemUIView*>(
      list_view->contents()->children().at(0));
  ASSERT_TRUE(item_view);

  auto* footer_view = item_view->footer_view_for_testing();
  ASSERT_NE(nullptr, footer_view);
  EXPECT_TRUE(footer_view->GetVisible());

  auto* detailed_view = item_view->view_for_testing();
  ASSERT_NE(nullptr, detailed_view);
  auto* pip_button = detailed_view->GetActionButtonForTesting(
      media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
  ASSERT_NE(nullptr, pip_button);
  EXPECT_FALSE(pip_button->GetVisible());

  // Click on the "Stop casting" button and ensure TerminateRoute is called.
  EXPECT_CALL(*media_router_, TerminateRoute(route_id));
  views::Button* stop_casting_button =
      static_cast<views::Button*>(footer_view->children()[0]);
  ASSERT_TRUE(stop_casting_button);
  views::test::ButtonTestApi(stop_casting_button)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0));

  // Route ends.
  EXPECT_CALL(*media_router_, GetCurrentRoutes())
      .WillRepeatedly(testing::Return(std::vector<media_router::MediaRoute>{}));
  provider->RefreshMediaItem(
      request_id.ToString(),
      provider->media_session_item_producer_for_testing()->GetMediaItem(
          request_id.ToString()));

  EXPECT_EQ(nullptr, item_view->footer_view_for_testing());
  EXPECT_TRUE(pip_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsCastStartRemotePlaybackTest,
                       StopCastingLocalMediaSessionRemotePlayback) {
  content::WebContents* web_contents =
      browser()->GetActiveTabInterface()->GetContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  content::MediaSession::Get(web_contents);
  auto request_id =
      content::MediaSession::GetRequestIdFromWebContents(web_contents);

  auto* provider = static_cast<MediaNotificationProviderImpl*>(
      MediaNotificationProvider::Get());
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->is_controllable = true;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video/vp8", "audio/opus",
          /*remote_playback_disabled=*/false,
          /*remote_playback_started=*/true,
          /*unused_field=*/std::nullopt,
          /*is_encrypted_media=*/false);

  media_session::mojom::AudioFocusRequestStatePtr focus(
      media_session::mojom::AudioFocusRequestState::New());
  focus->request_id = request_id;
  focus->session_info = std::move(session_info);

  provider->media_session_item_producer_for_testing()->OnFocusGained(
      std::move(focus));
  provider->media_session_item_producer_for_testing()->ActivateItem(
      request_id.ToString());

  const std::string route_id = "route_remote_playback";
  media_router::MediaRoute route(
      route_id,
      media_router::MediaSource::ForRemotePlayback(
          tab_id, media::VideoCodec::kVP8, media::AudioCodec::kOpus),
      "sink_1", "Test Remote Playback Route", true);
  route.set_media_sink_name("Living Room TV");
  EXPECT_CALL(*media_router_, GetCurrentRoutes())
      .WillRepeatedly(
          testing::Return(std::vector<media_router::MediaRoute>{route}));

  MediaTray* media_tray = Shell::GetPrimaryRootWindowController()
                              ->shelf()
                              ->GetStatusAreaWidget()
                              ->media_tray();
  media_tray->ShowBubble();

  auto* list_view = static_cast<global_media_controls::MediaItemUIListView*>(
      media_tray->content_view_for_testing());
  ASSERT_TRUE(list_view);
  ASSERT_EQ(1u, list_view->contents()->children().size());

  auto* item_view = static_cast<global_media_controls::MediaItemUIView*>(
      list_view->contents()->children().at(0));
  ASSERT_TRUE(item_view);

  auto* footer_view = item_view->footer_view_for_testing();
  ASSERT_NE(nullptr, footer_view);
  EXPECT_TRUE(footer_view->GetVisible());

  auto* detailed_view = item_view->view_for_testing();
  ASSERT_NE(nullptr, detailed_view);
  auto* pip_button = detailed_view->GetActionButtonForTesting(
      media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
  ASSERT_NE(nullptr, pip_button);
  EXPECT_FALSE(pip_button->GetVisible());

  // Click on the "Stop casting" button and ensure TerminateRoute is called.
  EXPECT_CALL(*media_router_, TerminateRoute(route_id));
  views::Button* stop_casting_button =
      static_cast<views::Button*>(footer_view->children()[0]);
  ASSERT_TRUE(stop_casting_button);
  views::test::ButtonTestApi(stop_casting_button)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0));

  // Route ends.
  EXPECT_CALL(*media_router_, GetCurrentRoutes())
      .WillRepeatedly(testing::Return(std::vector<media_router::MediaRoute>{}));
  provider->RefreshMediaItem(
      request_id.ToString(),
      provider->media_session_item_producer_for_testing()->GetMediaItem(
          request_id.ToString()));

  EXPECT_EQ(nullptr, item_view->footer_view_for_testing());
  EXPECT_TRUE(pip_button->GetVisible());
}

}  // namespace ash
