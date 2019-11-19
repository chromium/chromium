// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_views_ui.h"

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "url/origin.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {

constexpr char kPseudoSinkId[] = "pseudo:sink";
constexpr char kRouteId[] = "route1";
constexpr char kSinkDescription[] = "description";
constexpr char kSinkId[] = "sink1";
constexpr char kSinkName[] = "sink name";
constexpr char kSourceId[] = "source1";

ACTION_TEMPLATE(SaveArgWithMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(::testing::get<k>(args));
}

class MockControllerObserver : public CastDialogController::Observer {
 public:
  MockControllerObserver() = default;
  explicit MockControllerObserver(CastDialogController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }

  ~MockControllerObserver() override {
    if (controller_)
      controller_->RemoveObserver(this);
  }

  MOCK_METHOD1(OnModelUpdated, void(const CastDialogModel& model));
  void OnControllerInvalidated() {
    controller_ = nullptr;
    OnControllerInvalidatedInternal();
  }
  MOCK_METHOD0(OnControllerInvalidatedInternal, void());

 private:
  CastDialogController* controller_ = nullptr;
};

class MockMediaRouterFileDialog : public MediaRouterFileDialog {
 public:
  MockMediaRouterFileDialog() : MediaRouterFileDialog(nullptr) {}
  ~MockMediaRouterFileDialog() override {}

  MOCK_METHOD0(GetLastSelectedFileUrl, GURL());
  MOCK_METHOD0(GetLastSelectedFileName, base::string16());
  MOCK_METHOD1(OpenFileDialog, void(Browser* browser));
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

// Injects a MediaRouter instance into MediaRouterViewsUI.
class TestMediaRouterViewsUI : public MediaRouterViewsUI {
 public:
  explicit TestMediaRouterViewsUI(MediaRouter* router) : router_(router) {}
  ~TestMediaRouterViewsUI() override = default;

  MediaRouter* GetMediaRouter() const override { return router_; }

 private:
  MediaRouter* router_;
  DISALLOW_COPY_AND_ASSIGN(TestMediaRouterViewsUI);
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

}  // namespace

class MediaRouterViewsUITest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Store sink observers so that they can be notified in tests.
    ON_CALL(mock_router_, RegisterMediaSinksObserver(_))
        .WillByDefault(Invoke([this](MediaSinksObserver* observer) {
          media_sinks_observers_.push_back(observer);
          return true;
        }));

    SessionTabHelper::CreateForWebContents(web_contents());
    ui_ = std::make_unique<TestMediaRouterViewsUI>(&mock_router_);
    ui_->InitWithDefaultMediaSource(web_contents(), nullptr);
  }

  void TearDown() override {
    ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateMediaRouterUIForURL(const GURL& url) {
    web_contents()->GetController().LoadURL(url, content::Referrer(),
                                            ui::PAGE_TRANSITION_LINK, "");
    content::RenderFrameHostTester::CommitPendingLoad(
        &web_contents()->GetController());
    SessionTabHelper::CreateForWebContents(web_contents());
    ui_ = std::make_unique<TestMediaRouterViewsUI>(&mock_router_);
    ui_->InitWithDefaultMediaSource(web_contents(), nullptr);
  }

  // These methods are used so that we don't have to friend each test case that
  // calls the private methods.
  void NotifyUiOnResultsUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) {
    ui_->OnResultsUpdated(sinks);
  }
  void NotifyUiOnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) {
    ui_->OnRoutesUpdated(routes, joinable_route_ids);
  }

  void StartTabCasting(bool is_incognito) {
    MediaSource media_source =
        MediaSource::ForTab(SessionTabHelper::IdForTab(web_contents()).id());
    EXPECT_CALL(
        mock_router_,
        CreateRouteInternal(media_source.id(), kSinkId, _, web_contents(), _,
                            base::TimeDelta::FromSeconds(60), is_incognito));
    MediaSink sink(kSinkId, kSinkName, SinkIconType::GENERIC);
    for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
      sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
    ui_->StartCasting(kSinkId, MediaCastMode::TAB_MIRROR);
  }

  void StartCastingAndExpectTimeout(MediaCastMode cast_mode,
                                    const std::string& expected_issue_title,
                                    int timeout_seconds) {
    MockControllerObserver observer(ui_.get());
    MediaSink sink(kSinkId, kSinkName, SinkIconType::CAST);
    ui_->OnResultsUpdated({{sink, {cast_mode}}});
    MediaRouteResponseCallback callback;
    EXPECT_CALL(mock_router_,
                CreateRouteInternal(
                    _, _, _, _, _,
                    base::TimeDelta::FromSeconds(timeout_seconds), false))
        .WillOnce(SaveArgWithMove<4>(&callback));
    for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
      sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
    ui_->StartCasting(kSinkId, cast_mode);
    Mock::VerifyAndClearExpectations(&mock_router_);

    EXPECT_CALL(observer, OnModelUpdated(_))
        .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
          EXPECT_EQ(model.media_sinks()[0].issue->info().title,
                    expected_issue_title);
        }));
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Timed out", RouteRequestResult::TIMED_OUT);
    std::move(callback).Run(nullptr, *result);
  }

  // The caller must hold on to PresentationRequestCallbacks returned so that
  // a callback can later be called on it.
  std::unique_ptr<PresentationRequestCallbacks> ExpectPresentationError(
      blink::mojom::PresentationErrorType error_type,
      const std::string& error_message) {
    blink::mojom::PresentationError expected_error(error_type, error_message);
    auto request_callbacks =
        std::make_unique<PresentationRequestCallbacks>(expected_error);
    auto context = std::make_unique<StartPresentationContext>(
        presentation_request_,
        base::Bind(&PresentationRequestCallbacks::Success,
                   base::Unretained(request_callbacks.get())),
        base::Bind(&PresentationRequestCallbacks::Error,
                   base::Unretained(request_callbacks.get())));
    StartPresentationContext* context_ptr = context.get();
    ui_->set_start_presentation_context_for_test(std::move(context));
    ui_->OnDefaultPresentationChanged(context_ptr->presentation_request());
    return request_callbacks;
  }

 protected:
  std::vector<MediaSinksObserver*> media_sinks_observers_;
  MockMediaRouter mock_router_;
  std::unique_ptr<MediaRouterViewsUI> ui_;
  std::unique_ptr<StartPresentationContext> start_presentation_context_;
  content::PresentationRequest presentation_request_{
      {0, 0},
      {GURL("https://google.com/presentation")},
      url::Origin::Create(GURL("http://google.com"))};
};

TEST_F(MediaRouterViewsUITest, NotifyObserver) {
  MockControllerObserver observer;

  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        EXPECT_TRUE(model.media_sinks().empty());
      })));
  ui_->AddObserver(&observer);

  MediaSink sink(kSinkId, kSinkName, SinkIconType::CAST_AUDIO);
  MediaSinkWithCastModes sink_with_cast_modes(sink);
  sink_with_cast_modes.cast_modes = {MediaCastMode::TAB_MIRROR};
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([&sink](const CastDialogModel& model) {
        EXPECT_EQ(1u, model.media_sinks().size());
        const UIMediaSink& ui_sink = model.media_sinks()[0];
        EXPECT_EQ(sink.id(), ui_sink.id);
        EXPECT_EQ(base::UTF8ToUTF16(sink.name()), ui_sink.friendly_name);
        EXPECT_EQ(UIMediaSinkState::AVAILABLE, ui_sink.state);
        EXPECT_TRUE(
            base::Contains(ui_sink.cast_modes, MediaCastMode::TAB_MIRROR));
        EXPECT_EQ(sink.icon_type(), ui_sink.icon_type);
      })));
  NotifyUiOnResultsUpdated({sink_with_cast_modes});

  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true, true);
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(
          WithArg<0>(Invoke([&sink, &route](const CastDialogModel& model) {
            EXPECT_EQ(1u, model.media_sinks().size());
            const UIMediaSink& ui_sink = model.media_sinks()[0];
            EXPECT_EQ(sink.id(), ui_sink.id);
            EXPECT_EQ(UIMediaSinkState::CONNECTED, ui_sink.state);
            EXPECT_EQ(route.media_route_id(), ui_sink.route->media_route_id());
          })));
  NotifyUiOnRoutesUpdated({route}, {});

  EXPECT_CALL(observer, OnControllerInvalidatedInternal());
  ui_.reset();
}

TEST_F(MediaRouterViewsUITest, SinkFriendlyName) {
  MockControllerObserver observer(ui_.get());

  MediaSink sink(kSinkId, kSinkName, SinkIconType::CAST);
  sink.set_description(kSinkDescription);
  MediaSinkWithCastModes sink_with_cast_modes(sink);
  const char* separator = u8" \u2010 ";
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(Invoke([&](const CastDialogModel& model) {
        EXPECT_EQ(base::UTF8ToUTF16(sink.name() + separator +
                                    sink.description().value()),
                  model.media_sinks()[0].friendly_name);
      }));
  NotifyUiOnResultsUpdated({sink_with_cast_modes});
}

TEST_F(MediaRouterViewsUITest, SetDialogHeader) {
  MockControllerObserver observer;
  // Initially, the dialog header should simply say "Cast".
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce([&](const CastDialogModel& model) {
        EXPECT_EQ(
            l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE),
            model.dialog_header());
      });
  ui_->AddObserver(&observer);
  // We temporarily remove the observer here because the implementation calls
  // OnModelUpdated() multiple times when the presentation request gets set.
  ui_->RemoveObserver(&observer);

  GURL gurl("https://example.com");
  url::Origin origin = url::Origin::Create(gurl);
  ui_->OnDefaultPresentationChanged(content::PresentationRequest(
      content::GlobalFrameRoutingId(), {gurl}, origin));

  // Now that the presentation request has been set, the dialog header contains
  // its origin.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce([&](const CastDialogModel& model) {
        EXPECT_EQ(
            l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_PRESENTATION_CAST_MODE,
                                       base::UTF8ToUTF16(origin.host())),
            model.dialog_header());
      });
  ui_->AddObserver(&observer);
  ui_->RemoveObserver(&observer);
}

TEST_F(MediaRouterViewsUITest, StartCasting) {
  StartTabCasting(false);
}

TEST_F(MediaRouterViewsUITest, StopCasting) {
  EXPECT_CALL(mock_router_, TerminateRoute(kRouteId));
  ui_->StopCasting(kRouteId);
}

TEST_F(MediaRouterViewsUITest, RemovePseudoSink) {
  MockControllerObserver observer(ui_.get());

  MediaSink sink(kSinkId, kSinkName, SinkIconType::CAST_AUDIO);
  MediaSinkWithCastModes sink_with_cast_modes(sink);
  sink_with_cast_modes.cast_modes = {MediaCastMode::TAB_MIRROR};
  MediaSink pseudo_sink(kPseudoSinkId, kSinkName, SinkIconType::MEETING);
  MediaSinkWithCastModes pseudo_sink_with_cast_modes(pseudo_sink);
  pseudo_sink_with_cast_modes.cast_modes = {MediaCastMode::TAB_MIRROR};

  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([&sink](const CastDialogModel& model) {
        EXPECT_EQ(1u, model.media_sinks().size());
        EXPECT_EQ(sink.id(), model.media_sinks()[0].id);
      })));
  NotifyUiOnResultsUpdated({sink_with_cast_modes, pseudo_sink_with_cast_modes});
}

TEST_F(MediaRouterViewsUITest, ConnectingState) {
  MockControllerObserver observer(ui_.get());

  MediaSink sink(kSinkId, kSinkName, SinkIconType::GENERIC);
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
    sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());

  // When a request to Cast to a sink is made, its state should become
  // CONNECTING.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        ASSERT_EQ(1u, model.media_sinks().size());
        EXPECT_EQ(UIMediaSinkState::CONNECTING, model.media_sinks()[0].state);
      })));
  ui_->StartCasting(kSinkId, MediaCastMode::TAB_MIRROR);

  // Once a route is created for the sink, its state should become CONNECTED.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        ASSERT_EQ(1u, model.media_sinks().size());
        EXPECT_EQ(UIMediaSinkState::CONNECTED, model.media_sinks()[0].state);
      })));
  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true, true);
  NotifyUiOnRoutesUpdated({route}, {});
}

TEST_F(MediaRouterViewsUITest, DisconnectingState) {
  MockControllerObserver observer(ui_.get());

  MediaSink sink(kSinkId, kSinkName, SinkIconType::GENERIC);
  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true, true);
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
    sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
  NotifyUiOnRoutesUpdated({route}, {});

  // When a request to stop casting to a sink is made, its state should become
  // DISCONNECTING.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        ASSERT_EQ(1u, model.media_sinks().size());
        EXPECT_EQ(UIMediaSinkState::DISCONNECTING,
                  model.media_sinks()[0].state);
      })));
  ui_->StopCasting(kRouteId);

  // Once the route is removed, the sink's state should become AVAILABLE.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        ASSERT_EQ(1u, model.media_sinks().size());
        EXPECT_EQ(UIMediaSinkState::AVAILABLE, model.media_sinks()[0].state);
      })));
  NotifyUiOnRoutesUpdated({}, {});
}

TEST_F(MediaRouterViewsUITest, AddAndRemoveIssue) {
  MediaSink sink1("sink_id1", "Sink 1", SinkIconType::CAST_AUDIO);
  MediaSink sink2("sink_id2", "Sink 2", SinkIconType::CAST_AUDIO);
  NotifyUiOnResultsUpdated({{sink1, {MediaCastMode::TAB_MIRROR}},
                            {sink2, {MediaCastMode::TAB_MIRROR}}});

  MockControllerObserver observer(ui_.get());
  MockIssuesObserver issues_observer(mock_router_.GetIssueManager());
  issues_observer.Init();
  const std::string issue_title("Issue 1");
  IssueInfo issue(issue_title, IssueInfo::Action::DISMISS,
                  IssueInfo::Severity::WARNING);
  issue.sink_id = sink2.id();
  Issue::Id issue_id = -1;

  EXPECT_CALL(issues_observer, OnIssue)
      .WillOnce(
          Invoke([&issue_id](const Issue& issue) { issue_id = issue.id(); }));
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(
          Invoke([&sink1, &sink2, &issue_title](const CastDialogModel& model) {
            EXPECT_EQ(2u, model.media_sinks().size());
            EXPECT_EQ(model.media_sinks()[0].id, sink1.id());
            EXPECT_FALSE(model.media_sinks()[0].issue.has_value());
            EXPECT_EQ(model.media_sinks()[1].id, sink2.id());
            EXPECT_EQ(model.media_sinks()[1].issue->info().title, issue_title);
          })));
  mock_router_.GetIssueManager()->AddIssue(issue);

  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([&sink2](const CastDialogModel& model) {
        EXPECT_EQ(2u, model.media_sinks().size());
        EXPECT_EQ(model.media_sinks()[1].id, sink2.id());
        EXPECT_FALSE(model.media_sinks()[1].issue.has_value());
      })));
  mock_router_.GetIssueManager()->ClearIssue(issue_id);
}

TEST_F(MediaRouterViewsUITest, ShowDomainForHangouts) {
  const std::string domain1 = "domain1.com";
  const std::string domain2 = "domain2.com";
  MediaSinkWithCastModes available_hangout(
      MediaSink("sink1", "Hangout 1", SinkIconType::HANGOUT));
  MediaSinkWithCastModes connected_hangout(
      MediaSink("sink2", "Hangout 2", SinkIconType::HANGOUT));
  available_hangout.sink.set_domain(domain1);
  connected_hangout.sink.set_domain(domain2);
  available_hangout.cast_modes = {MediaCastMode::TAB_MIRROR};
  connected_hangout.cast_modes = {MediaCastMode::TAB_MIRROR};

  MockControllerObserver observer(ui_.get());
  const std::string route_description = "route 1";
  MediaRoute route(kRouteId, MediaSource(kSourceId), "sink2", route_description,
                   true, true);
  NotifyUiOnRoutesUpdated({route}, {});

  // The domain should be used as the status text only if the sink is available.
  // If the sink has a route, the route description is used.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
        EXPECT_EQ(2u, model.media_sinks().size());
        EXPECT_EQ(model.media_sinks()[0].id, available_hangout.sink.id());
        EXPECT_EQ(base::UTF8ToUTF16(domain1),
                  model.media_sinks()[0].status_text);
        EXPECT_EQ(model.media_sinks()[1].id, connected_hangout.sink.id());
        EXPECT_EQ(base::UTF8ToUTF16(route_description),
                  model.media_sinks()[1].status_text);
      }));
  NotifyUiOnResultsUpdated({available_hangout, connected_hangout});
}

TEST_F(MediaRouterViewsUITest, RouteCreationTimeoutForTab) {
  StartCastingAndExpectTimeout(
      MediaCastMode::TAB_MIRROR,
      l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB),
      60);
}

TEST_F(MediaRouterViewsUITest, RouteCreationTimeoutForDesktop) {
  StartCastingAndExpectTimeout(
      MediaCastMode::DESKTOP_MIRROR,
      l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_DESKTOP),
      120);
}

TEST_F(MediaRouterViewsUITest, RouteCreationTimeoutForPresentation) {
  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://presentationurl.com")},
      url::Origin::Create(GURL("https://frameurl.fakeurl")));
  ui_->OnDefaultPresentationChanged(presentation_request);
  StartCastingAndExpectTimeout(
      MediaCastMode::PRESENTATION,
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT,
                                base::UTF8ToUTF16("frameurl.fakeurl")),
      20);
}

// Tests that if a local file CreateRoute call is made from a new tab, the
// file will be opened in the new tab.
TEST_F(MediaRouterViewsUITest, RouteCreationLocalFileModeInTab) {
  const GURL empty_tab = GURL(chrome::kChromeUINewTabURL);
  const std::string file_url = "file:///some/url/for/a/file.mp3";
  CreateMediaRouterUIForURL(empty_tab);
  auto file_dialog = std::make_unique<MockMediaRouterFileDialog>();
  auto* file_dialog_ptr = file_dialog.get();
  ui_->set_media_router_file_dialog_for_test(std::move(file_dialog));

  EXPECT_CALL(*file_dialog_ptr, GetLastSelectedFileUrl())
      .WillOnce(Return(GURL(file_url)));
  content::WebContents* location_file_opened = nullptr;
  EXPECT_CALL(mock_router_, CreateRouteInternal(_, _, _, _, _, _, _))
      .WillOnce(SaveArgWithMove<3>(&location_file_opened));
  ui_->CreateRoute(kSinkId, MediaCastMode::LOCAL_FILE);

  ASSERT_EQ(location_file_opened, web_contents());
  ASSERT_EQ(location_file_opened->GetVisibleURL(), file_url);
}

TEST_F(MediaRouterViewsUITest, SortedSinks) {
  NotifyUiOnResultsUpdated(
      {{MediaSink("sink3", "B sink", SinkIconType::CAST), {}},
       {MediaSink("sink2", "A sink", SinkIconType::CAST), {}},
       {MediaSink("sink1", "B sink", SinkIconType::CAST), {}}});

  // Sort first by name, then by ID.
  const auto& sorted_sinks = ui_->GetEnabledSinks();
  EXPECT_EQ("sink2", sorted_sinks[0].sink.id());
  EXPECT_EQ("sink1", sorted_sinks[1].sink.id());
  EXPECT_EQ("sink3", sorted_sinks[2].sink.id());
}

TEST_F(MediaRouterViewsUITest, SortSinksByIconType) {
  NotifyUiOnResultsUpdated(
      {{MediaSink("id1", "sink", SinkIconType::HANGOUT), {}},
       {MediaSink("id2", "B sink", SinkIconType::CAST_AUDIO_GROUP), {}},
       {MediaSink("id3", "sink", SinkIconType::GENERIC), {}},
       {MediaSink("id4", "A sink", SinkIconType::CAST_AUDIO_GROUP), {}},
       {MediaSink("id5", "sink", SinkIconType::CAST_AUDIO), {}},
       {MediaSink("id6", "sink", SinkIconType::CAST), {}}});

  // The sorted order is CAST, CAST_AUDIO_GROUP "A", CAST_AUDIO_GROUP "B",
  // CAST_AUDIO, HANGOUT, GENERIC.
  const auto& sorted_sinks = ui_->GetEnabledSinks();
  EXPECT_EQ("id6", sorted_sinks[0].sink.id());
  EXPECT_EQ("id4", sorted_sinks[1].sink.id());
  EXPECT_EQ("id2", sorted_sinks[2].sink.id());
  EXPECT_EQ("id5", sorted_sinks[3].sink.id());
  EXPECT_EQ("id1", sorted_sinks[4].sink.id());
  EXPECT_EQ("id3", sorted_sinks[5].sink.id());
}

TEST_F(MediaRouterViewsUITest, FilterNonDisplayRoutes) {
  MediaSource media_source("mediaSource");
  MediaRoute display_route_1("routeId1", media_source, "sinkId1", "desc 1",
                             true, true);
  MediaRoute non_display_route_1("routeId2", media_source, "sinkId2", "desc 2",
                                 true, false);
  MediaRoute display_route_2("routeId3", media_source, "sinkId2", "desc 2",
                             true, true);

  NotifyUiOnRoutesUpdated(
      {display_route_1, non_display_route_1, display_route_2}, {});
  ASSERT_EQ(2u, ui_->routes().size());
  EXPECT_EQ(display_route_1, ui_->routes()[0]);
  EXPECT_TRUE(ui_->routes()[0].for_display());
  EXPECT_EQ(display_route_2, ui_->routes()[1]);
  EXPECT_TRUE(ui_->routes()[1].for_display());
}

TEST_F(MediaRouterViewsUITest, NotFoundErrorOnCloseWithNoSinks) {
  auto request_callbacks = ExpectPresentationError(
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");
  // Destroying the UI should return the expected error from above to the error
  // callback.
  ui_.reset();
}

TEST_F(MediaRouterViewsUITest, NotFoundErrorOnCloseWithNoCompatibleSinks) {
  auto request_callbacks = ExpectPresentationError(
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");
  // Send a sink to the UI that is compatible with sources other than the
  // presentation url to cause a NotFoundError.
  std::vector<MediaSink> sinks = {{kSinkId, kSinkName, SinkIconType::GENERIC}};
  auto presentation_source = MediaSource::ForPresentationUrl(
      presentation_request_.presentation_urls[0]);
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_) {
    if (!(sinks_observer->source() == presentation_source)) {
      sinks_observer->OnSinksUpdated(sinks, {});
    }
  }
  // Destroying the UI should return the expected error from above to the error
  // callback.
  ui_.reset();
}

TEST_F(MediaRouterViewsUITest, AbortErrorOnClose) {
  auto request_callbacks = ExpectPresentationError(
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
      "Dialog closed.");
  // Send a sink to the UI that is compatible with the presentation url to avoid
  // a NotFoundError.
  std::vector<MediaSink> sinks = {{kSinkId, kSinkName, SinkIconType::GENERIC}};
  auto presentation_source = MediaSource::ForPresentationUrl(
      presentation_request_.presentation_urls[0]);
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_) {
    if (sinks_observer->source() == presentation_source) {
      sinks_observer->OnSinksUpdated(sinks, {});
    }
  }
  // Destroying the UI should return the expected error from above to the error
  // callback.
  ui_.reset();
}

// A wired display sink should not be on the sinks list when the dialog is on
// that display, to prevent showing a fullscreen presentation window over the
// controlling window.
TEST_F(MediaRouterViewsUITest, UpdateSinksWhenDialogMovesToAnotherDisplay) {
  MockControllerObserver observer(ui_.get());
  const display::Display display1(1000001);
  const display::Display display2(1000002);
  const std::string display_sink_id1 =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display1);
  const std::string display_sink_id2 =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display2);

  auto display_observer_unique =
      std::make_unique<TestWebContentsDisplayObserver>(display1);
  TestWebContentsDisplayObserver* display_observer =
      display_observer_unique.get();
  ui_->display_observer_ = std::move(display_observer_unique);

  NotifyUiOnResultsUpdated(
      {{MediaSink(display_sink_id1, "sink", SinkIconType::GENERIC), {}},
       {MediaSink(display_sink_id2, "sink", SinkIconType::GENERIC), {}},
       {MediaSink("id3", "sink", SinkIconType::GENERIC), {}}});

  // Initially |display_sink_id1| should not be on the sinks list because we are
  // on |display1|.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
        const auto& sinks = model.media_sinks();
        EXPECT_EQ(2u, sinks.size());
        EXPECT_TRUE(std::find_if(sinks.begin(), sinks.end(),
                                 [&](const UIMediaSink& sink) {
                                   return sink.id == display_sink_id1;
                                 }) == sinks.end());
      }));
  ui_->UpdateSinks();
  Mock::VerifyAndClearExpectations(&observer);

  // Change the display to |display2|. Now |display_sink_id2| should be removed
  // from the list of sinks.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
        const auto& sinks = model.media_sinks();
        EXPECT_EQ(2u, sinks.size());
        EXPECT_TRUE(std::find_if(sinks.begin(), sinks.end(),
                                 [&](const UIMediaSink& sink) {
                                   return sink.id == display_sink_id2;
                                 }) == sinks.end());
      }));
  display_observer->set_display(display2);
  ui_->UpdateSinks();
}

class MediaRouterViewsUIIncognitoTest : public MediaRouterViewsUITest {
 protected:
  content::BrowserContext* GetBrowserContext() override {
    return static_cast<Profile*>(MediaRouterViewsUITest::GetBrowserContext())
        ->GetOffTheRecordProfile();
  }
};

TEST_F(MediaRouterViewsUIIncognitoTest, HidesCloudSinksForIncognito) {
  const std::string domain1 = "domain1.com";
  MediaSinkWithCastModes hangout(
      MediaSink("sink1", "Hangout", SinkIconType::HANGOUT));
  MediaSinkWithCastModes meeting(
      MediaSink("sink2", "Meeting", SinkIconType::MEETING));
  MediaSinkWithCastModes eduReceiver(
      MediaSink("sink3", "Cast for EDU", SinkIconType::EDUCATION));
  MediaSinkWithCastModes chromeCast(
      MediaSink("sink4", "Living Room TV", SinkIconType::CAST));
  chromeCast.cast_modes = {MediaCastMode::TAB_MIRROR};
  for (MediaSinkWithCastModes* sink :
       std::initializer_list<MediaSinkWithCastModes*>{&hangout, &meeting,
                                                      &eduReceiver}) {
    sink->sink.set_domain(domain1);
    sink->cast_modes = {MediaCastMode::TAB_MIRROR};
  }

  NotifyUiOnResultsUpdated({hangout, meeting, eduReceiver, chromeCast});

  EXPECT_EQ(std::vector<MediaSinkWithCastModes>{chromeCast},
            ui_->GetEnabledSinks());
}

TEST_F(MediaRouterViewsUIIncognitoTest, RouteRequestFromIncognito) {
  StartTabCasting(true);
}

}  // namespace media_router
