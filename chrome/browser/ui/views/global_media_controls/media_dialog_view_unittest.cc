// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/soda_installer_impl.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "components/global_media_controls/public/test/mock_media_session_notification_item_delegate.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class MediaDialogViewTest : public ChromeViewsTestBase {
 public:
  MediaDialogViewTest() = default;
  MediaDialogViewTest(const MediaDialogViewTest&) = delete;
  MediaDialogViewTest& operator=(const MediaDialogViewTest&) = delete;
  ~MediaDialogViewTest() override = default;

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
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    soda_installer_impl_ = std::make_unique<speech::SodaInstallerImpl>();

    MediaDialogView::ShowDialogFromToolbar(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        notification_service_.get(), profile());
    view_ = MediaDialogView::GetDialogViewForTesting();
  }

  void TearDown() override {
    HideDialog();
    anchor_widget_->Close();
    ChromeViewsTestBase::TearDown();
  }

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
        std::move(session_info), /*always_hidden=*/false);
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

  global_media_controls::MediaItemUIUpdatedView* media_item_ui_updated_view() {
    return view_->GetItemsForTesting().begin()->second;
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }

  void HideDialog() {
    views::Widget* widget =
        MediaDialogView::IsShowing()
            ? MediaDialogView::GetDialogViewForTesting()->GetWidget()
            : nullptr;
    MediaDialogView::HideDialog();
    view_ = nullptr;
    // Widget::Close() is asynchronous, so the dialog would otherwise outlive
    // the objects it points at.
    if (widget) {
      views::test::WidgetDestroyedWaiter(widget).Wait();
    }
  }

  // TEST_F() subclasses do not inherit this fixture's friendship with
  // `MediaDialogView`.
  views::ToggleButton* live_caption_button(MediaDialogView* view) {
    return view->live_caption_button_;
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

TEST_F(MediaDialogViewTest, BuildDeviceSelectorView_RemotePlaybackSource) {
  auto item = SimulateMediaSessionNotificationItem();

  view()->ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_FALSE(media_item_ui_updated_view()->GetFooterForTesting());
  EXPECT_TRUE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());

  SimulateMediaRouteUpdate({CreateRemotePlaybackRoute()});
  view()->RefreshMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_TRUE(media_item_ui_updated_view()->GetFooterForTesting());
  EXPECT_FALSE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
}

TEST_F(MediaDialogViewTest, BuildDeviceSelectorView_TabMirroringSource) {
  auto item = SimulateMediaSessionNotificationItem();
  SimulateMediaRouteUpdate({CreateTabMirroringRoute()});

  view()->ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_TRUE(media_item_ui_updated_view()->GetFooterForTesting());
  EXPECT_FALSE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
}

TEST_F(MediaDialogViewTest, TerminateSession) {
  auto item = SimulateMediaSessionNotificationItem();
  SimulateMediaRouteUpdate({CreateRemotePlaybackRoute()});

  view()->ShowMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  auto* footer_view = media_item_ui_updated_view()->GetFooterForTesting();
  EXPECT_TRUE(footer_view && footer_view->GetVisible());
  EXPECT_FALSE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());

  EXPECT_CALL(*media_router(),
              TerminateRoute(CreateRemotePlaybackRoute().media_route_id()));
  views::Button* stop_casting_button = static_cast<views::Button*>(
      media_item_ui_updated_view()->GetFooterForTesting()->children()[2]);
  views::test::ButtonTestApi(stop_casting_button)
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(0, 0), gfx::Point(0, 0),
          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));

  SimulateMediaRouteUpdate({});
  view()->RefreshMediaItem(
      content::MediaSession::GetRequestIdFromWebContents(web_contents())
          .ToString(),
      item->GetWeakPtr());
  EXPECT_FALSE(media_item_ui_updated_view()->GetFooterForTesting());
  EXPECT_TRUE(media_item_ui_updated_view()->GetDeviceSelectorForTesting());
}

// Regression test for crbug.com/468238180. The dialog reads and writes the
// original profile's prefs, so it must observe that same profile.
TEST_F(MediaDialogViewTest, LiveCaptionUpdatesInIncognito) {
  if (!captions::IsLiveCaptionFeatureSupported()) {
    GTEST_SKIP() << "Live Caption is not supported on this platform.";
  }
  // This test drives its own incognito dialog.
  HideDialog();

  // Enabling the pref would otherwise build caption bubble UI, which needs a
  // root window. The factory redirects off-the-record profiles to the
  // original, so the override has to go there.
  captions::LiveCaptionControllerFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));

  Profile* incognito = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
      incognito, base::BindRepeating(&media_router::MockMediaRouter::Create));
  auto incognito_service =
      std::make_unique<MediaNotificationService>(incognito, false);

  // Writing through the off-the-record PrefService shadows the pref, which
  // suppresses later notifications from the original profile.
  incognito->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, false);

  MediaDialogView::ShowDialogFromToolbar(
      views::BubbleAnchor(anchor_widget()->GetContentsView()),
      incognito_service.get(), incognito);
  MediaDialogView* view = MediaDialogView::GetDialogViewForTesting();
  ASSERT_TRUE(live_caption_button(view));
  ASSERT_FALSE(live_caption_button(view)->GetIsOn());

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  ASSERT_FALSE(incognito->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(live_caption_button(view)->GetIsOn());

  HideDialog();
}
