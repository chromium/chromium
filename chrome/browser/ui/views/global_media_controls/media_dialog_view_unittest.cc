// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/soda_installer_impl.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "components/global_media_controls/public/test/mock_media_session_notification_item_delegate.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
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

class MediaDialogViewTest : public ChromeViewsTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  MediaDialogViewTest() = default;
  MediaDialogViewTest(const MediaDialogViewTest&) = delete;
  MediaDialogViewTest& operator=(const MediaDialogViewTest&) = delete;
  ~MediaDialogViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    feature_list_.InitWithFeatureState(media::kGlobalMediaControlsUpdatedUI,
                                       UseUpdatedUI());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));
    media_router_ = static_cast<media_router::MockMediaRouter*>(
        media_router::MediaRouterFactory::GetApiForBrowserContext(profile()));

    notification_service_ =
        std::make_unique<MediaNotificationService>(profile(), false);
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    soda_installer_impl_ = std::make_unique<speech::SodaInstallerImpl>();

    MediaDialogView::ShowDialogFromToolbar(anchor_widget_->GetContentsView(),
                                           notification_service_.get(),
                                           profile());
    view_ = MediaDialogView::GetDialogViewForTesting();
  }

  void TearDown() override {
    MediaDialogView::HideDialog();
    view_ = nullptr;
    anchor_widget_->Close();
    ChromeViewsTestBase::TearDown();
  }

  bool UseUpdatedUI() { return GetParam(); }

  std::unique_ptr<global_media_controls::MediaSessionNotificationItem>
  SimulateMediaSessionNotificationItem() {
    auto session_info = media_session::mojom::MediaSessionInfo::New();
    session_info->remote_playback_metadata =
        media_session::mojom::RemotePlaybackMetadata::New(
            "video_codec", "audio_codec", false, true, "device_friendly_name",
            false);
    content::MediaSession::Get(web_contents());
    return std::make_unique<
        global_media_controls::MediaSessionNotificationItem>(
        &delegate_,
        content::MediaSession::GetRequestIdFromWebContents(web_contents())
            .ToString(),
        "source_name", std::nullopt, controller_.CreateMediaControllerRemote(),
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

  global_media_controls::MediaItemUIView* media_item_ui_view() {
    return view_->GetItemsForTesting().begin()->second;
  }

  global_media_controls::MediaItemUIUpdatedView* media_item_ui_updated_view() {
    return view_->GetUpdatedItemsForTesting().begin()->second;
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_.get(); }
  media_router::MockMediaRouter* media_router() { return media_router_; }
  MediaDialogView* view() { return view_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MediaNotificationService> notification_service_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<MediaDialogView> view_;
  media_session::test::TestMediaController controller_;
  testing::NiceMock<
      global_media_controls::test::MockMediaSessionNotificationItemDelegate>
      delegate_;
  raw_ptr<media_router::MockMediaRouter> media_router_;
  std::unique_ptr<speech::SodaInstallerImpl> soda_installer_impl_;
};

INSTANTIATE_TEST_SUITE_P(GlobalMediaControlsUpdatedUI,
                         MediaDialogViewTest,
                         testing::Bool());

TEST_P(MediaDialogViewTest, BuildDeviceSelectorView_RemotePlaybackSource) {
  auto item = SimulateMediaSessionNotificationItem();

  view()->ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  if (UseUpdatedUI()) {
    EXPECT_FALSE(media_item_ui_updated_view()->GetFooterForTesting());
    EXPECT_TRUE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
  } else {
    EXPECT_FALSE(media_item_ui_view()->footer_view_for_testing());
    EXPECT_TRUE(media_item_ui_view()->device_selector_view_for_testing());
  }

  SimulateMediaRouteUpdate({CreateRemotePlaybackRoute()});
  view()->RefreshMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  if (UseUpdatedUI()) {
    EXPECT_TRUE(media_item_ui_updated_view()->GetFooterForTesting());
    EXPECT_FALSE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
  } else {
    EXPECT_TRUE(media_item_ui_view()->footer_view_for_testing());
    EXPECT_FALSE(media_item_ui_view()->device_selector_view_for_testing());
  }
}

TEST_P(MediaDialogViewTest, BuildDeviceSelectorView_TabMirroringSource) {
  auto item = SimulateMediaSessionNotificationItem();
  SimulateMediaRouteUpdate({CreateTabMirroringRoute()});

  view()->ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  if (UseUpdatedUI()) {
    EXPECT_TRUE(media_item_ui_updated_view()->GetFooterForTesting());
    EXPECT_FALSE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
  } else {
    EXPECT_TRUE(media_item_ui_view()->footer_view_for_testing());
    EXPECT_FALSE(media_item_ui_view()->device_selector_view_for_testing());
  }
}

TEST_P(MediaDialogViewTest, TerminateSession) {
  auto item = SimulateMediaSessionNotificationItem();
  SimulateMediaRouteUpdate({CreateRemotePlaybackRoute()});

  view()->ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  if (UseUpdatedUI()) {
    auto* footer_view = media_item_ui_updated_view()->GetFooterForTesting();
    EXPECT_TRUE(footer_view && footer_view->GetVisible());
    EXPECT_FALSE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
  } else {
    auto* footer_view = media_item_ui_view()->footer_view_for_testing();
    EXPECT_TRUE(footer_view && footer_view->GetVisible());
    EXPECT_FALSE(media_item_ui_view()->device_selector_view_for_testing());
  }

  EXPECT_CALL(*media_router(),
              TerminateRoute(CreateRemotePlaybackRoute().media_route_id()));
  views::Button* stop_casting_button;
  if (UseUpdatedUI()) {
    stop_casting_button = static_cast<views::Button*>(
        media_item_ui_updated_view()->GetFooterForTesting()->children()[2]);
  } else {
    stop_casting_button = static_cast<views::Button*>(
        media_item_ui_view()->footer_view_for_testing()->children()[0]);
  }
  views::test::ButtonTestApi(stop_casting_button)
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(0, 0), gfx::Point(0, 0),
          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));

  SimulateMediaRouteUpdate({});
  view()->RefreshMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  if (UseUpdatedUI()) {
    EXPECT_FALSE(media_item_ui_updated_view()->GetFooterForTesting());
    EXPECT_TRUE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
  } else {
    EXPECT_FALSE(media_item_ui_view()->footer_view_for_testing());
    EXPECT_TRUE(media_item_ui_view()->device_selector_view_for_testing());
  }
}
