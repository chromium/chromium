// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/soda_installer_impl.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "components/global_media_controls/public/test/mock_media_session_notification_item_delegate.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

class MediaDialogViewWithRemotePlaybackTest : public ChromeViewsTestBase {
 public:
  MediaDialogViewWithRemotePlaybackTest() {
    feature_list_.InitAndEnableFeature(media::kMediaRemotingWithoutFullscreen);
  }
  ~MediaDialogViewWithRemotePlaybackTest() override = default;

 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));
    media_router_ = static_cast<media_router::MockMediaRouter*>(
        media_router::MediaRouterFactory::GetApiForBrowserContext(profile()));

    notification_service_ =
        std::make_unique<MediaNotificationService>(profile(), false);
    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    soda_installer_impl_ = std::make_unique<speech::SodaInstallerImpl>();

    MediaDialogView::ShowDialogFromToolbar(anchor_widget_->GetContentsView(),
                                           notification_service_.get(),
                                           profile());
    view_ = MediaDialogView::GetDialogViewForTesting();
  }

  void TearDown() override {
    MediaDialogView::HideDialog();
    anchor_widget_->Close();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<global_media_controls::MediaSessionNotificationItem>
  SimulateMediaSessionNotificationItem() {
    auto session_info = media_session::mojom::MediaSessionInfo::New();
    session_info->remote_playback_metadata =
        media_session::mojom::RemotePlaybackMetadata::New(
            "video_codec", "audio_codec", false, true, "device_friendly_name");
    content::MediaSession::Get(web_contents());
    return std::make_unique<
        global_media_controls::MediaSessionNotificationItem>(
        &delegate_,
        content::MediaSession::GetRequestIdFromWebContents(web_contents())
            .ToString(),
        "source_name", controller_.CreateMediaControllerRemote(),
        std::move(session_info));
  }

  void SimulateMediaRouteUpdate(std::vector<media_router::MediaRoute> routes) {
    ON_CALL(*media_router_, GetCurrentRoutes())
        .WillByDefault(testing::Return(routes));
    base::RunLoop().RunUntilIdle();
  }

  const media_router::MediaRoute CreateRemotePlaybackRoute() {
    media_router::MediaRoute route(
        "id",
        media_router::MediaSource(base::StringPrintf(
            "remote-playback:media-session?tab_id=%d&"
            "video_codec=hevc&audio_codec=aac",
            sessions::SessionTabHelper::IdForTab(web_contents()).id())),
        "sink_id", "route_description", true);
    route.set_media_sink_name("My sink");
    return route;
  }

  const media_router::MediaRoute CreateTabMirroringRoute() {
    media_router::MediaRoute route(
        "id",
        media_router::MediaSource(base::StringPrintf(
            "urn:x-org.chromium.media:source:tab:%d",
            sessions::SessionTabHelper::IdForTab(web_contents()).id())),
        "sink_id", "route_description", true);
    route.set_media_sink_name("My sink");
    return route;
  }

  std::unique_ptr<global_media_controls::MediaItemUIView> BuildMediaItemUIView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) {
    return view_->BuildMediaItemUIView(id, item);
  }

  void RefreshMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) {
    return view_->RefreshMediaItem(id, item);
  }

  global_media_controls::MediaItemUIView* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) {
    view_->ShowMediaItem(id, item);
    return view_->GetItemsForTesting().begin()->second;
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_.get(); }
  media_router::MockMediaRouter* media_router() { return media_router_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MediaNotificationService> notification_service_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<MediaDialogView> view_ = nullptr;
  media_session::test::TestMediaController controller_;
  testing::NiceMock<
      global_media_controls::test::MockMediaSessionNotificationItemDelegate>
      delegate_;
  raw_ptr<media_router::MockMediaRouter> media_router_;
  std::unique_ptr<speech::SodaInstallerImpl> soda_installer_impl_;
};

TEST_F(MediaDialogViewWithRemotePlaybackTest,
       BuildDeviceSelectorView_RemotePlaybackSource) {
  auto item = SimulateMediaSessionNotificationItem();

  auto* media_item_ui_view = ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_FALSE(media_item_ui_view->footer_view_for_testing());
  EXPECT_TRUE(media_item_ui_view->device_selector_view_for_testing());

  SimulateMediaRouteUpdate({CreateRemotePlaybackRoute()});
  RefreshMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_TRUE(media_item_ui_view->footer_view_for_testing());
  EXPECT_FALSE(media_item_ui_view->device_selector_view_for_testing());
}

TEST_F(MediaDialogViewWithRemotePlaybackTest,
       BuildDeviceSelectorView_TabMirroringSource) {
  auto item = SimulateMediaSessionNotificationItem();
  SimulateMediaRouteUpdate({CreateTabMirroringRoute()});

  auto* media_item_ui_view = ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_TRUE(media_item_ui_view->footer_view_for_testing());
  EXPECT_FALSE(media_item_ui_view->device_selector_view_for_testing());
}

TEST_F(MediaDialogViewWithRemotePlaybackTest, TerminateSession) {
  auto item = SimulateMediaSessionNotificationItem();
  SimulateMediaRouteUpdate({CreateRemotePlaybackRoute()});

  auto* media_item_ui_view = ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  auto* footer_view = media_item_ui_view->footer_view_for_testing();
  EXPECT_TRUE(footer_view && footer_view->GetVisible());

  // Click on the "Stop Casting button".
  EXPECT_CALL(*media_router(),
              TerminateRoute(CreateRemotePlaybackRoute().media_route_id()));
  views::Button* stop_casting_button =
      static_cast<views::Button*>(footer_view->children()[0]);
  views::test::ButtonTestApi(stop_casting_button)
      .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                                  gfx::Point(0, 0), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0));
}
