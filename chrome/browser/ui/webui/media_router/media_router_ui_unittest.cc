// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "chrome/browser/ui/webui/media_router/web_contents_display_observer.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"

using content::WebContents;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Mock;
using testing::Return;

namespace media_router {

ACTION_TEMPLATE(SaveArgWithMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(::testing::get<k>(args));
}

class MockMediaRouterWebUIMessageHandler
    : public MediaRouterWebUIMessageHandler {
 public:
  explicit MockMediaRouterWebUIMessageHandler(MediaRouterUI* media_router_ui)
      : MediaRouterWebUIMessageHandler(media_router_ui) {}
  ~MockMediaRouterWebUIMessageHandler() override {}

  MOCK_METHOD1(UpdateSinks,
               void(const std::vector<MediaSinkWithCastModes>& sinks));
  MOCK_METHOD1(UpdateIssue, void(const Issue& issue));
  MOCK_METHOD1(UpdateMediaRouteStatus, void(const MediaStatus& status));
  MOCK_METHOD3(UpdateCastModes,
               void(const CastModeSet& cast_modes,
                    const std::string& source_host,
                    base::Optional<MediaCastMode> forced_cast_mode));
};

class MockMediaRouterFileDialog : public MediaRouterFileDialog {
 public:
  MockMediaRouterFileDialog() : MediaRouterFileDialog(nullptr) {}
  ~MockMediaRouterFileDialog() override {}

  MOCK_METHOD0(GetLastSelectedFileUrl, GURL());
  MOCK_METHOD0(GetLastSelectedFileName, base::string16());
  MOCK_METHOD1(OpenFileDialog, void(Browser* browser));
};

class TestWebContentsDisplayObserver : public WebContentsDisplayObserver {
 public:
  explicit TestWebContentsDisplayObserver(const display::Display& display)
      : display_(display) {}
  ~TestWebContentsDisplayObserver() override {}

  const display::Display& GetCurrentDisplay() const override {
    return display_;
  }

  void set_display(const display::Display& display) { display_ = display; }

 private:
  display::Display display_;
};

class PresentationRequestCallbacks {
 public:
  PresentationRequestCallbacks() {}

  explicit PresentationRequestCallbacks(
      const blink::mojom::PresentationError& expected_error)
      : expected_error_(expected_error) {}

  void Success(const blink::mojom::PresentationInfo&,
               mojom::RoutePresentationConnectionPtr,
               const MediaRoute&) {}

  void Error(const blink::mojom::PresentationError& error) {
    EXPECT_EQ(expected_error_.error_type, error.error_type);
    EXPECT_EQ(expected_error_.message, error.message);
  }

 private:
  blink::mojom::PresentationError expected_error_;
};

class TestMediaRouterUI : public MediaRouterUI {
 public:
  TestMediaRouterUI(content::WebUI* web_ui, MediaRouter* router)
      : MediaRouterUI(web_ui), router_(router) {}
  ~TestMediaRouterUI() override = default;

 private:
  MediaRouter* GetMediaRouter() const override { return router_; }

  MediaRouter* router_;
  DISALLOW_COPY_AND_ASSIGN(TestMediaRouterUI);
};

class MediaRouterUITest : public ChromeRenderViewHostTestHarness {
 public:
  MediaRouterUITest()
      : presentation_request_({0, 0},
                              {GURL("https://google.com/presentation")},
                              url::Origin::Create(GURL("http://google.com"))) {
    // enable and disable features
    scoped_feature_list_.InitFromCommandLine(
        "EnableCastLocalMedia" /* enabled features */,
        std::string() /* disabled features */);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_CALL(mock_router_, OnUserGesture()).Times(AnyNumber());
    EXPECT_CALL(mock_router_, GetCurrentRoutes())
        .Times(AnyNumber())
        .WillRepeatedly(Return(std::vector<MediaRoute>()));
  }

  void TearDown() override {
    EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_))
        .Times(AnyNumber());
    web_ui_contents_.reset();
    start_presentation_context_.reset();
    media_router_ui_.reset();
    message_handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateMediaRouterUIForURL(Profile* profile, const GURL& url) {
    web_contents()->GetController().LoadURL(url, content::Referrer(),
                                            ui::PAGE_TRANSITION_LINK, "");
    content::RenderFrameHostTester::CommitPendingLoad(
        &web_contents()->GetController());
    CreateMediaRouterUI(profile);
  }

  void CreateMediaRouterUI(Profile* profile) {
    SessionTabHelper::CreateForWebContents(web_contents());
    web_ui_contents_ = WebContents::Create(WebContents::CreateParams(profile));
    web_ui_.set_web_contents(web_ui_contents_.get());
    media_router_ui_ =
        std::make_unique<TestMediaRouterUI>(&web_ui_, &mock_router_);
    message_handler_ = std::make_unique<MockMediaRouterWebUIMessageHandler>(
        media_router_ui_.get());

    auto file_dialog = std::make_unique<MockMediaRouterFileDialog>();
    mock_file_dialog_ = file_dialog.get();

    EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
        .WillRepeatedly(Invoke([this](MediaSinksObserver* observer) {
          this->media_sinks_observers_.push_back(observer);
          return true;
        }));
    EXPECT_CALL(mock_router_, RegisterMediaRoutesObserver(_))
        .Times(AnyNumber());
    media_router_ui_->InitForTest(
        &mock_router_, web_contents(), message_handler_.get(),
        std::move(start_presentation_context_), std::move(file_dialog));
    message_handler_->SetWebUIForTest(&web_ui_);
  }

  MediaSink CreateSinkCompatibleWithAllSources() {
    MediaSink sink("sinkId", "sinkName", SinkIconType::GENERIC);
    for (auto* observer : media_sinks_observers_)
      observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
    return sink;
  }

  // Notifies MediaRouterUI that a route details view has been opened. Expects
  // MediaRouterUI to request a MediaRouteController, and gives it a mock
  // controller. Returns a reference to the mock controller.
  scoped_refptr<MockMediaRouteController> OpenUIDetailsView(
      const MediaRoute::Id& route_id) {
    auto controller = base::MakeRefCounted<MockMediaRouteController>(
        route_id, profile(), &mock_router_);
    MediaSource media_source("mediaSource");
    MediaRoute route(route_id, media_source, "sinkId", "", true, true);

    media_router_ui_->OnRoutesUpdated({route}, std::vector<MediaRoute::Id>());
    EXPECT_CALL(mock_router_, GetRouteController(route_id))
        .WillOnce(Return(controller));
    media_router_ui_->OnMediaControllerUIAvailable(route_id);

    return controller;
  }

 protected:
  MockMediaRouter mock_router_;
  content::PresentationRequest presentation_request_;
  content::TestWebUI web_ui_;
  std::unique_ptr<WebContents> web_ui_contents_;
  std::unique_ptr<StartPresentationContext> start_presentation_context_;
  std::unique_ptr<TestMediaRouterUI> media_router_ui_;
  std::unique_ptr<MockMediaRouterWebUIMessageHandler> message_handler_;
  MockMediaRouterFileDialog* mock_file_dialog_ = nullptr;
  std::vector<MediaSinksObserver*> media_sinks_observers_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MediaRouterUIIncognitoTest : public MediaRouterUITest {
 protected:
  content::BrowserContext* GetBrowserContext() override {
    return static_cast<Profile*>(MediaRouterUITest::GetBrowserContext())
        ->GetOffTheRecordProfile();
  }
};

TEST_F(MediaRouterUITest, RouteCreationTimeoutForTab) {
  CreateMediaRouterUI(profile());
  MediaRouteResponseCallback callback;
  EXPECT_CALL(mock_router_,
              CreateRouteInternal(_, _, _, _, _,
                                  base::TimeDelta::FromSeconds(60), false))
      .WillOnce(SaveArgWithMove<4>(&callback));
  media_router_ui_->CreateRoute(CreateSinkCompatibleWithAllSources().id(),
                                MediaCastMode::TAB_MIRROR);

  std::string expected_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
  EXPECT_CALL(*message_handler_, UpdateIssue(IssueTitleEquals(expected_title)));
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError("Timed out", RouteRequestResult::TIMED_OUT);
  std::move(callback).Run(nullptr, *result);
}

TEST_F(MediaRouterUITest, RouteCreationTimeoutForDesktop) {
  CreateMediaRouterUI(profile());
  MediaRouteResponseCallback callback;
  EXPECT_CALL(mock_router_,
              CreateRouteInternal(_, _, _, _, _,
                                  base::TimeDelta::FromSeconds(120), false))
      .WillOnce(SaveArgWithMove<4>(&callback));
  media_router_ui_->CreateRoute(CreateSinkCompatibleWithAllSources().id(),
                                MediaCastMode::DESKTOP_MIRROR);

  std::string expected_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_DESKTOP);
  EXPECT_CALL(*message_handler_, UpdateIssue(IssueTitleEquals(expected_title)));
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError("Timed out", RouteRequestResult::TIMED_OUT);
  std::move(callback).Run(nullptr, *result);
}

TEST_F(MediaRouterUITest, RouteCreationTimeoutForPresentation) {
  CreateMediaRouterUI(profile());
  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://presentationurl.com")},
      url::Origin::Create(GURL("https://frameurl.fakeurl")));
  media_router_ui_->OnDefaultPresentationChanged(presentation_request);
  MediaRouteResponseCallback callback;
  EXPECT_CALL(mock_router_,
              CreateRouteInternal(_, _, _, _, _,
                                  base::TimeDelta::FromSeconds(20), false))
      .WillOnce(SaveArgWithMove<4>(&callback));
  media_router_ui_->CreateRoute(CreateSinkCompatibleWithAllSources().id(),
                                MediaCastMode::PRESENTATION);

  std::string expected_title =
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT,
                                base::UTF8ToUTF16("frameurl.fakeurl"));
  EXPECT_CALL(*message_handler_, UpdateIssue(IssueTitleEquals(expected_title)));
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError("Timed out", RouteRequestResult::TIMED_OUT);
  std::move(callback).Run(nullptr, *result);
}

// Tests that if a local file CreateRoute call is made from a new tab, the
// file will be opened in the new tab.
TEST_F(MediaRouterUITest, RouteCreationLocalFileModeInTab) {
  const GURL empty_tab = GURL(chrome::kChromeUINewTabURL);
  const std::string file_url = "file:///some/url/for/a/file.mp3";

  // Setup the UI
  CreateMediaRouterUIForURL(profile(), empty_tab);

  EXPECT_CALL(*mock_file_dialog_, GetLastSelectedFileUrl())
      .WillOnce(Return(GURL(file_url)));

  content::WebContents* location_file_opened = nullptr;

  // Expect that the media_router_ will make a call to the mock_router
  // then we will want to check that it made the call with.
  EXPECT_CALL(mock_router_, CreateRouteInternal(_, _, _, _, _, _, _))
      .WillOnce(SaveArgWithMove<3>(&location_file_opened));

  media_router_ui_->CreateRoute(CreateSinkCompatibleWithAllSources().id(),
                                MediaCastMode::LOCAL_FILE);

  ASSERT_EQ(location_file_opened, web_contents());
  ASSERT_EQ(location_file_opened->GetVisibleURL(), file_url);
}

TEST_F(MediaRouterUITest, RouteCreationParametersCantBeCreated) {
  CreateMediaRouterUI(profile());
  MediaSinkSearchResponseCallback sink_callback;
  EXPECT_CALL(mock_router_, SearchSinksInternal(_, _, _, _, _))
      .WillOnce(SaveArgWithMove<4>(&sink_callback));

  // Use PRESENTATION mode without setting a PresentationRequest.
  media_router_ui_->SearchSinksAndCreateRoute(
      "sinkId", "search input", "domain", MediaCastMode::PRESENTATION);
  std::string expected_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
  EXPECT_CALL(*message_handler_, UpdateIssue(IssueTitleEquals(expected_title)));
  std::move(sink_callback).Run("foundSinkId");
}

TEST_F(MediaRouterUIIncognitoTest, RouteRequestFromIncognito) {
  CreateMediaRouterUI(profile());
  media_router_ui_->OnDefaultPresentationChanged(presentation_request_);

  EXPECT_CALL(mock_router_,
              CreateRouteInternal(_, _, _, _, _,
                                  base::TimeDelta::FromSeconds(20), true));
  media_router_ui_->CreateRoute(CreateSinkCompatibleWithAllSources().id(),
                                MediaCastMode::PRESENTATION);
}

TEST_F(MediaRouterUITest, SortedSinks) {
  CreateMediaRouterUI(profile());
  std::vector<MediaSinkWithCastModes> unsorted_sinks;
  std::string sink_id1("sink3");
  std::string sink_name1("B sink");
  MediaSinkWithCastModes sink1(
      MediaSink(sink_id1, sink_name1, SinkIconType::CAST));
  unsorted_sinks.push_back(sink1);

  std::string sink_id2("sink1");
  std::string sink_name2("A sink");
  MediaSinkWithCastModes sink2(
      MediaSink(sink_id2, sink_name2, SinkIconType::CAST));
  unsorted_sinks.push_back(sink2);

  std::string sink_id3("sink2");
  std::string sink_name3("B sink");
  MediaSinkWithCastModes sink3(
      MediaSink(sink_id3, sink_name3, SinkIconType::CAST));
  unsorted_sinks.push_back(sink3);

  // Sorted order is 2, 3, 1.
  media_router_ui_->OnResultsUpdated(unsorted_sinks);
  const auto& sorted_sinks = media_router_ui_->GetEnabledSinks();
  EXPECT_EQ(sink_name2, sorted_sinks[0].sink.name());
  EXPECT_EQ(sink_id3, sorted_sinks[1].sink.id());
  EXPECT_EQ(sink_id1, sorted_sinks[2].sink.id());
}

TEST_F(MediaRouterUITest, SortSinksByIconType) {
  CreateMediaRouterUI(profile());
  std::vector<MediaSinkWithCastModes> unsorted_sinks;

  MediaSinkWithCastModes sink1(MediaSink("id1", "sink", SinkIconType::HANGOUT));
  unsorted_sinks.push_back(sink1);
  MediaSinkWithCastModes sink2(
      MediaSink("id2", "B sink", SinkIconType::CAST_AUDIO_GROUP));
  unsorted_sinks.push_back(sink2);
  MediaSinkWithCastModes sink3(MediaSink("id3", "sink", SinkIconType::GENERIC));
  unsorted_sinks.push_back(sink3);
  MediaSinkWithCastModes sink4(
      MediaSink("id4", "A sink", SinkIconType::CAST_AUDIO_GROUP));
  unsorted_sinks.push_back(sink4);
  MediaSinkWithCastModes sink5(
      MediaSink("id5", "sink", SinkIconType::CAST_AUDIO));
  unsorted_sinks.push_back(sink5);
  MediaSinkWithCastModes sink6(MediaSink("id6", "sink", SinkIconType::CAST));
  unsorted_sinks.push_back(sink6);

  // Sorted order is CAST, CAST_AUDIO_GROUP "A", CAST_AUDIO_GROUP "B",
  // CAST_AUDIO, HANGOUT, GENERIC.
  media_router_ui_->OnResultsUpdated(unsorted_sinks);
  const auto& sorted_sinks = media_router_ui_->GetEnabledSinks();
  EXPECT_EQ(sink6.sink.id(), sorted_sinks[0].sink.id());
  EXPECT_EQ(sink4.sink.id(), sorted_sinks[1].sink.id());
  EXPECT_EQ(sink2.sink.id(), sorted_sinks[2].sink.id());
  EXPECT_EQ(sink5.sink.id(), sorted_sinks[3].sink.id());
  EXPECT_EQ(sink1.sink.id(), sorted_sinks[4].sink.id());
  EXPECT_EQ(sink3.sink.id(), sorted_sinks[5].sink.id());
}

TEST_F(MediaRouterUITest, FilterNonDisplayRoutes) {
  CreateMediaRouterUI(profile());

  MediaSource media_source("mediaSource");
  MediaRoute display_route_1("routeId1", media_source, "sinkId1", "desc 1",
                             true, true);
  MediaRoute non_display_route_1("routeId2", media_source, "sinkId2", "desc 2",
                                 true, false);
  MediaRoute display_route_2("routeId3", media_source, "sinkId2", "desc 2",
                             true, true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  media_router_ui_->OnRoutesUpdated(routes, std::vector<MediaRoute::Id>());
  ASSERT_EQ(2u, media_router_ui_->routes().size());
  EXPECT_TRUE(display_route_1.Equals(media_router_ui_->routes()[0]));
  EXPECT_TRUE(media_router_ui_->routes()[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(media_router_ui_->routes()[1]));
  EXPECT_TRUE(media_router_ui_->routes()[1].for_display());
}

TEST_F(MediaRouterUITest, FilterNonDisplayJoinableRoutes) {
  CreateMediaRouterUI(profile());

  MediaSource media_source("mediaSource");
  MediaRoute display_route_1("routeId1", media_source, "sinkId1", "desc 1",
                             true, true);
  MediaRoute non_display_route_1("routeId2", media_source, "sinkId2", "desc 2",
                                 true, false);
  MediaRoute display_route_2("routeId3", media_source, "sinkId2", "desc 2",
                             true, true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  std::vector<MediaRoute::Id> joinable_route_ids;
  joinable_route_ids.push_back("routeId1");
  joinable_route_ids.push_back("routeId2");
  joinable_route_ids.push_back("routeId3");

  media_router_ui_->OnRoutesUpdated(routes, joinable_route_ids);
  ASSERT_EQ(2u, media_router_ui_->joinable_route_ids().size());
  EXPECT_EQ(display_route_1.media_route_id(),
            media_router_ui_->joinable_route_ids()[0]);
  EXPECT_EQ(display_route_2.media_route_id(),
            media_router_ui_->joinable_route_ids()[1]);
}

TEST_F(MediaRouterUITest, UIMediaRoutesObserverAssignsCurrentCastModes) {
  CreateMediaRouterUI(profile());
  SessionID tab_id = SessionTabHelper::IdForTab(web_contents());
  MediaSource media_source_1(MediaSourceForTab(tab_id.id()));
  MediaSource media_source_2("mediaSource");
  MediaSource media_source_3(MediaSourceForDesktop());
  std::unique_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router_, MediaSource::Id(),
          base::Bind(&MediaRouterUI::OnRoutesUpdated,
                     base::Unretained(media_router_ui_.get()))));

  MediaRoute display_route_1("routeId1", media_source_1, "sinkId1", "desc 1",
                             true, true);
  MediaRoute non_display_route_1("routeId2", media_source_2, "sinkId2",
                                 "desc 2", true, false);
  MediaRoute display_route_2("routeId3", media_source_3, "sinkId2", "desc 2",
                             true, true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  observer->OnRoutesUpdated(routes, std::vector<MediaRoute::Id>());

  const auto& filtered_routes = media_router_ui_->routes();
  ASSERT_EQ(2u, filtered_routes.size());
  EXPECT_TRUE(display_route_1.Equals(filtered_routes[0]));
  EXPECT_TRUE(filtered_routes[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(filtered_routes[1]));
  EXPECT_TRUE(filtered_routes[1].for_display());

  const auto& current_cast_modes = media_router_ui_->routes_and_cast_modes();
  ASSERT_EQ(2u, current_cast_modes.size());
  auto cast_mode_entry =
      current_cast_modes.find(display_route_1.media_route_id());
  EXPECT_NE(end(current_cast_modes), cast_mode_entry);
  EXPECT_EQ(MediaCastMode::TAB_MIRROR, cast_mode_entry->second);
  cast_mode_entry =
      current_cast_modes.find(non_display_route_1.media_route_id());
  EXPECT_EQ(end(current_cast_modes), cast_mode_entry);
  cast_mode_entry = current_cast_modes.find(display_route_2.media_route_id());
  EXPECT_NE(end(current_cast_modes), cast_mode_entry);
  EXPECT_EQ(MediaCastMode::DESKTOP_MIRROR, cast_mode_entry->second);

  EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest, UIMediaRoutesObserverSkipsUnavailableCastModes) {
  CreateMediaRouterUI(profile());
  MediaSource media_source_1("mediaSource1");
  MediaSource media_source_2("mediaSource2");
  MediaSource media_source_3(MediaSourceForDesktop());
  std::unique_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router_, MediaSource::Id(),
          base::Bind(&MediaRouterUI::OnRoutesUpdated,
                     base::Unretained(media_router_ui_.get()))));

  MediaRoute display_route_1("routeId1", media_source_1, "sinkId1", "desc 1",
                             true, true);
  MediaRoute non_display_route_1("routeId2", media_source_2, "sinkId2",
                                 "desc 2", true, false);
  MediaRoute display_route_2("routeId3", media_source_3, "sinkId2", "desc 2",
                             true, true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  observer->OnRoutesUpdated(routes, std::vector<MediaRoute::Id>());

  const auto& filtered_routes = media_router_ui_->routes();
  ASSERT_EQ(2u, filtered_routes.size());
  EXPECT_TRUE(display_route_1.Equals(filtered_routes[0]));
  EXPECT_TRUE(filtered_routes[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(filtered_routes[1]));
  EXPECT_TRUE(filtered_routes[1].for_display());

  const auto& current_cast_modes = media_router_ui_->routes_and_cast_modes();
  ASSERT_EQ(1u, current_cast_modes.size());
  auto cast_mode_entry =
      current_cast_modes.find(display_route_1.media_route_id());
  // No observer for source "mediaSource1" means no cast mode for this route.
  EXPECT_EQ(end(current_cast_modes), cast_mode_entry);
  cast_mode_entry =
      current_cast_modes.find(non_display_route_1.media_route_id());
  EXPECT_EQ(end(current_cast_modes), cast_mode_entry);
  cast_mode_entry = current_cast_modes.find(display_route_2.media_route_id());
  EXPECT_NE(end(current_cast_modes), cast_mode_entry);
  EXPECT_EQ(MediaCastMode::DESKTOP_MIRROR, cast_mode_entry->second);

  EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest, NotFoundErrorOnCloseWithNoSinks) {
  blink::mojom::PresentationError expected_error(
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  start_presentation_context_ = std::make_unique<StartPresentationContext>(
      presentation_request_,
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks)));
  CreateMediaRouterUI(profile());
  // Destroying the UI should return the expected error from above to the error
  // callback.
  media_router_ui_.reset();
}

TEST_F(MediaRouterUITest, NotFoundErrorOnCloseWithNoCompatibleSinks) {
  blink::mojom::PresentationError expected_error(
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  start_presentation_context_ = std::make_unique<StartPresentationContext>(
      presentation_request_,
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks)));
  CreateMediaRouterUI(profile());

  // Send a sink to the UI that is compatible with sources other than the
  // presentation url to cause a NotFoundError.
  std::vector<MediaSink> sinks;
  sinks.emplace_back("sink id", "sink name", SinkIconType::GENERIC);
  std::vector<url::Origin> origins;
  auto presentation_source =
      MediaSourceForPresentationUrl(presentation_request_.presentation_urls[0]);
  for (auto* observer : media_sinks_observers_) {
    if (!(observer->source() == presentation_source)) {
      observer->OnSinksUpdated(sinks, origins);
    }
  }
  // Destroying the UI should return the expected error from above to the error
  // callback.
  media_router_ui_.reset();
}

TEST_F(MediaRouterUITest, AbortErrorOnClose) {
  blink::mojom::PresentationError expected_error(
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
      "Dialog closed.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  start_presentation_context_ = std::make_unique<StartPresentationContext>(
      presentation_request_,
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks)));
  CreateMediaRouterUI(profile());

  // Send a sink to the UI that is compatible with the presentation url to avoid
  // a NotFoundError.
  std::vector<MediaSink> sinks;
  sinks.emplace_back("sink id", "sink name", SinkIconType::GENERIC);
  std::vector<url::Origin> origins;
  auto presentation_source =
      MediaSourceForPresentationUrl(presentation_request_.presentation_urls[0]);
  for (auto* observer : media_sinks_observers_) {
    if (observer->source() == presentation_source) {
      observer->OnSinksUpdated(sinks, origins);
    }
  }
  // Destroying the UI should return the expected error from above to the error
  // callback.
  media_router_ui_.reset();
}

TEST_F(MediaRouterUITest, RecordCastModeSelections) {
  const GURL url_1a = GURL("https://www.example.com/watch?v=AAAA");
  const GURL url_1b = GURL("https://www.example.com/watch?v=BBBB");
  const GURL url_2 = GURL("https://example2.com/0000");
  const GURL url_3 = GURL("https://www3.example.com/index.html");

  CreateMediaRouterUIForURL(profile(), url_1a);
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
  media_router_ui_->RecordCastModeSelection(MediaCastMode::TAB_MIRROR);
  EXPECT_TRUE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());

  CreateMediaRouterUIForURL(profile(), url_2);
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());

  CreateMediaRouterUIForURL(profile(), url_1b);
  // |url_1a| and |url_1b| have the same origin, so the selection made for
  // |url_1a| should be retrieved.
  EXPECT_TRUE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
  media_router_ui_->RecordCastModeSelection(MediaCastMode::PRESENTATION);
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());

  media_router_ui_->RecordCastModeSelection(MediaCastMode::TAB_MIRROR);
  CreateMediaRouterUIForURL(profile(), url_3);
  // |url_1a| and |url_3| have the same domain "example.com" but different
  // origins, so their preferences should be separate.
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
}

TEST_F(MediaRouterUITest, RecordCastModeSelectionsInIncognito) {
  const GURL url = GURL("https://www.example.com/watch?v=AAAA");

  CreateMediaRouterUIForURL(profile()->GetOffTheRecordProfile(), url);
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
  media_router_ui_->RecordCastModeSelection(MediaCastMode::TAB_MIRROR);
  EXPECT_TRUE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());

  // Selections recorded in incognito shouldn't be retrieved in the regular
  // profile.
  CreateMediaRouterUIForURL(profile(), url);
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
}

TEST_F(MediaRouterUITest, RecordDesktopMirroringCastModeSelection) {
  const GURL url = GURL("https://www.example.com/watch?v=AAAA");
  CreateMediaRouterUIForURL(profile(), url);

  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
  media_router_ui_->RecordCastModeSelection(MediaCastMode::DESKTOP_MIRROR);
  // Selecting desktop mirroring should not change the recorded preferences.
  EXPECT_FALSE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());

  media_router_ui_->RecordCastModeSelection(MediaCastMode::TAB_MIRROR);
  EXPECT_TRUE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
  media_router_ui_->RecordCastModeSelection(MediaCastMode::DESKTOP_MIRROR);
  // Selecting desktop mirroring should not change the recorded preferences.
  EXPECT_TRUE(media_router_ui_->UserSelectedTabMirroringForCurrentOrigin());
}

TEST_F(MediaRouterUITest, OpenAndCloseUIDetailsView) {
  const std::string route_id = "routeId";
  CreateMediaRouterUI(profile());
  OpenUIDetailsView(route_id);

  // When the route details view is closed, the route controller observer should
  // be destroyed, also triggering the destruction of the controller.
  EXPECT_CALL(mock_router_, DetachRouteController(route_id, _));
  media_router_ui_->OnMediaControllerUIClosed();

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_router_));
}

TEST_F(MediaRouterUITest, SendMediaStatusUpdate) {
  MediaStatus status;
  status.title = "test title";
  CreateMediaRouterUI(profile());
  scoped_refptr<MockMediaRouteController> controller =
      OpenUIDetailsView("routeId");

  // The route controller observer held by MediaRouterUI should send the status
  // update to the message handler.
  EXPECT_CALL(*message_handler_, UpdateMediaRouteStatus(status));
  controller->OnMediaStatusUpdated(status);

  // |controller| will outlive |mock_router_| because we passed it into
  // testing::Return(). Invalidate it so that it doesn't reference
  // |mock_router_| in its dtor.
  controller->Invalidate();
}

TEST_F(MediaRouterUITest, SendInitialMediaStatusUpdate) {
  MediaStatus status;
  status.title = "test title";
  std::string route_id = "routeId";
  auto controller = base::MakeRefCounted<MockMediaRouteController>(
      route_id, profile(), &mock_router_);
  controller->OnMediaStatusUpdated(status);

  CreateMediaRouterUI(profile());
  MediaSource media_source("mediaSource");
  MediaRoute route(route_id, media_source, "sinkId", "", true, true);
  media_router_ui_->OnRoutesUpdated({route}, std::vector<MediaRoute::Id>());

  // If the controller has already received a media status update, MediaRouterUI
  // should be notified with it when it starts observing the controller.
  EXPECT_CALL(mock_router_, GetRouteController(route_id))
      .WillOnce(Return(controller));
  EXPECT_CALL(*message_handler_, UpdateMediaRouteStatus(status));
  media_router_ui_->OnMediaControllerUIAvailable(route_id);

  // |controller| will outlive |mock_router_| because we passed it into
  // testing::Return(). Invalidate it so that it doesn't reference
  // |mock_router_| in its dtor.
  controller->Invalidate();
}

TEST_F(MediaRouterUITest, SetsForcedCastModeWithPresentationURLs) {
  presentation_request_.presentation_urls.push_back(
      GURL("https://google.com/presentation2"));
  blink::mojom::PresentationError expected_error(
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  start_presentation_context_ = std::make_unique<StartPresentationContext>(
      presentation_request_,
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks)));

  SessionTabHelper::CreateForWebContents(web_contents());
  web_ui_contents_ = WebContents::Create(WebContents::CreateParams(profile()));
  web_ui_.set_web_contents(web_ui_contents_.get());
  media_router_ui_ =
      std::make_unique<TestMediaRouterUI>(&web_ui_, &mock_router_);
  message_handler_ = std::make_unique<MockMediaRouterWebUIMessageHandler>(
      media_router_ui_.get());
  message_handler_->SetWebUIForTest(&web_ui_);
  EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
      .WillRepeatedly(Invoke([this](MediaSinksObserver* observer) {
        this->media_sinks_observers_.push_back(observer);
        return true;
      }));
  EXPECT_CALL(mock_router_, RegisterMediaRoutesObserver(_)).Times(AnyNumber());

  CastModeSet expected_modes(
      {MediaCastMode::TAB_MIRROR, MediaCastMode::DESKTOP_MIRROR,
       MediaCastMode::LOCAL_FILE, MediaCastMode::PRESENTATION});
  media_router_ui_->InitForTest(
      &mock_router_, web_contents(), message_handler_.get(),
      std::move(start_presentation_context_), nullptr);
  EXPECT_EQ(expected_modes, media_router_ui_->cast_modes());
  EXPECT_EQ(base::Optional<MediaCastMode>(MediaCastMode::PRESENTATION),
            media_router_ui_->forced_cast_mode());
  EXPECT_EQ("google.com", media_router_ui_->GetPresentationRequestSourceName());

  // |media_router_ui_| takes ownership of |request_callbacks|.
  media_router_ui_.reset();
}

// A wired display sink should not be on the sinks list when the dialog is on
// that display, to prevent showing a fullscreen presentation window over the
// controlling window.
TEST_F(MediaRouterUITest, UpdateSinksWhenDialogMovesToAnotherDisplay) {
  const display::Display display1(1000001);
  const display::Display display2(1000002);
  const std::string display_sink_id1 =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display1);
  const std::string display_sink_id2 =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display2);

  CreateMediaRouterUI(profile());

  auto display_observer_unique =
      std::make_unique<TestWebContentsDisplayObserver>(display1);
  TestWebContentsDisplayObserver* display_observer =
      display_observer_unique.get();
  media_router_ui_->set_display_observer_for_test(
      std::move(display_observer_unique));

  std::vector<MediaSinkWithCastModes> sinks;
  MediaSinkWithCastModes display_sink1(
      MediaSink(display_sink_id1, "sink", SinkIconType::GENERIC));
  sinks.push_back(display_sink1);
  MediaSinkWithCastModes display_sink2(
      MediaSink(display_sink_id2, "sink", SinkIconType::GENERIC));
  sinks.push_back(display_sink2);
  MediaSinkWithCastModes sink3(MediaSink("id3", "sink", SinkIconType::GENERIC));
  sinks.push_back(sink3);
  media_router_ui_->OnResultsUpdated(sinks);

  // Initially |display_sink1| should not be on the sinks list because we are on
  // |display1|.
  EXPECT_CALL(*message_handler_, UpdateSinks(_))
      .WillOnce(Invoke([&display_sink_id1](
                           const std::vector<MediaSinkWithCastModes>& sinks) {
        EXPECT_EQ(2u, sinks.size());
        EXPECT_TRUE(std::find_if(sinks.begin(), sinks.end(),
                                 [&display_sink_id1](
                                     const MediaSinkWithCastModes& sink) {
                                   return sink.sink.id() == display_sink_id1;
                                 }) == sinks.end());
      }));
  media_router_ui_->UpdateSinks();

  // Change the display to |display2|. Now |display_sink2| should be removed
  // from the list of sinks.
  EXPECT_CALL(*message_handler_, UpdateSinks(_))
      .WillOnce(Invoke([&display_sink_id2](
                           const std::vector<MediaSinkWithCastModes>& sinks) {
        EXPECT_EQ(2u, sinks.size());
        EXPECT_TRUE(std::find_if(sinks.begin(), sinks.end(),
                                 [&display_sink_id2](
                                     const MediaSinkWithCastModes& sink) {
                                   return sink.sink.id() == display_sink_id2;
                                 }) == sinks.end());
      }));
  display_observer->set_display(display2);
  media_router_ui_->UpdateSinks();
}

}  // namespace media_router
