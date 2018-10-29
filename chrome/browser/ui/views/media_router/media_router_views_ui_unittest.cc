// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_views_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace media_router {

namespace {

constexpr char kPseudoSinkId[] = "pseudo:sink";
constexpr char kRouteId[] = "route1";
constexpr char kSinkDescription[] = "description";
constexpr char kSinkId[] = "sink1";
constexpr char kSinkName[] = "sink name";
constexpr char kSourceId[] = "source1";

}  // namespace

class MockControllerObserver : public CastDialogController::Observer {
 public:
  MOCK_METHOD1(OnModelUpdated, void(const CastDialogModel& model));
  MOCK_METHOD0(OnControllerInvalidated, void());
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

 protected:
  std::vector<MediaSinksObserver*> media_sinks_observers_;
  MockMediaRouter mock_router_;
  std::unique_ptr<MediaRouterViewsUI> ui_;
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
            base::ContainsKey(ui_sink.cast_modes, MediaCastMode::TAB_MIRROR));
        EXPECT_EQ(sink.icon_type(), ui_sink.icon_type);
      })));
  ui_->OnResultsUpdated({sink_with_cast_modes});

  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true, true);
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(
          WithArg<0>(Invoke([&sink, &route](const CastDialogModel& model) {
            EXPECT_EQ(1u, model.media_sinks().size());
            const UIMediaSink& ui_sink = model.media_sinks()[0];
            EXPECT_EQ(sink.id(), ui_sink.id);
            EXPECT_EQ(UIMediaSinkState::CONNECTED, ui_sink.state);
            EXPECT_EQ(route.media_route_id(), ui_sink.route_id);
          })));
  ui_->OnRoutesUpdated({route}, {});

  EXPECT_CALL(observer, OnControllerInvalidated());
  ui_.reset();
}

TEST_F(MediaRouterViewsUITest, SinkFriendlyName) {
  MockControllerObserver observer;
  ui_->AddObserver(&observer);

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
  ui_->OnResultsUpdated({sink_with_cast_modes});
  ui_->RemoveObserver(&observer);
}

TEST_F(MediaRouterViewsUITest, SetDialogHeader) {
  MockControllerObserver observer;
  // Initially, the dialog header should simply say "Cast".
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce([&](const CastDialogModel& model) {
        EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_CAST_DIALOG_TITLE),
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
  MediaSource media_source =
      MediaSourceForTab(SessionTabHelper::IdForTab(web_contents()).id());
  EXPECT_CALL(mock_router_,
              CreateRouteInternal(media_source.id(), kSinkId, _, web_contents(),
                                  _, base::TimeDelta::FromSeconds(60), false));
  MediaSink sink(kSinkId, kSinkName, SinkIconType::GENERIC);
  for (MediaSinksObserver* observer : media_sinks_observers_)
    observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
  ui_->StartCasting(kSinkId, MediaCastMode::TAB_MIRROR);
}

TEST_F(MediaRouterViewsUITest, StopCasting) {
  EXPECT_CALL(mock_router_, TerminateRoute(kRouteId));
  ui_->StopCasting(kRouteId);
}

TEST_F(MediaRouterViewsUITest, RemovePseudoSink) {
  MockControllerObserver observer;
  ui_->AddObserver(&observer);

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
  ui_->OnResultsUpdated({sink_with_cast_modes, pseudo_sink_with_cast_modes});
  ui_->RemoveObserver(&observer);
}

TEST_F(MediaRouterViewsUITest, ConnectingState) {
  MockControllerObserver observer;
  ui_->AddObserver(&observer);

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
  ui_->OnRoutesUpdated({route}, {});
  ui_->RemoveObserver(&observer);
}

TEST_F(MediaRouterViewsUITest, DisconnectingState) {
  MockControllerObserver observer;
  ui_->AddObserver(&observer);

  MediaSink sink(kSinkId, kSinkName, SinkIconType::GENERIC);
  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true, true);
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
    sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
  ui_->OnRoutesUpdated({route}, {});

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
  ui_->OnRoutesUpdated({}, {});
  ui_->RemoveObserver(&observer);
}

TEST_F(MediaRouterViewsUITest, AddAndRemoveIssue) {
  MediaSink sink1("sink_id1", "Sink 1", SinkIconType::CAST_AUDIO);
  MediaSinkWithCastModes sink1_with_cast_modes(sink1);
  sink1_with_cast_modes.cast_modes = {MediaCastMode::TAB_MIRROR};
  MediaSink sink2("sink_id2", "Sink 2", SinkIconType::CAST_AUDIO);
  MediaSinkWithCastModes sink2_with_cast_modes(sink2);
  sink2_with_cast_modes.cast_modes = {MediaCastMode::TAB_MIRROR};
  ui_->OnResultsUpdated({sink1_with_cast_modes, sink2_with_cast_modes});

  MockControllerObserver observer;
  ui_->AddObserver(&observer);
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
  ui_->RemoveObserver(&observer);
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

  MockControllerObserver observer;
  ui_->AddObserver(&observer);
  const std::string route_description = "route 1";
  MediaRoute route(kRouteId, MediaSource(kSourceId), "sink2", route_description,
                   true, true);
  ui_->OnRoutesUpdated({route}, {});

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
  ui_->OnResultsUpdated({available_hangout, connected_hangout});
  ui_->RemoveObserver(&observer);
}

}  // namespace media_router
