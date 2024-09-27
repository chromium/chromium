// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui.h"

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/media_router/web_contents_display_observer.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/browser/test/test_helper.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/route_request_result.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "url/origin.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {

constexpr char kRouteId[] = "route1";
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

  MOCK_METHOD(void, OnModelUpdated, (const CastDialogModel& model), (override));
  MOCK_METHOD(void, OnCastingStarted, (), (override));
  void OnControllerDestroying() override {
    controller_ = nullptr;
    OnControllerDestroyingInternal();
  }
  MOCK_METHOD(void, OnControllerDestroyingInternal, ());

 private:
  raw_ptr<CastDialogController> controller_ = nullptr;
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

    SetMediaRouterFactory();
    mock_router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetApiForBrowserContext(GetBrowserContext()));
    logger_ = std::make_unique<LoggerImpl>();

    // Store sink observers so that they can be notified in tests.
    ON_CALL(*mock_router_, RegisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          media_sinks_observers_.push_back(observer);
          return true;
        });
    ON_CALL(*mock_router_, GetLogger()).WillByDefault(Return(logger_.get()));

    CreateSessionServiceTabHelper(web_contents());
    ui_ =
        MediaRouterUI::CreateWithDefaultMediaSourceAndMirroring(web_contents());
  }

  void TearDown() override {
#if BUILDFLAG(IS_MAC)
    clear_screen_capture_allowed_for_testing();
#endif
    ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual void SetMediaRouterFactory() {
    ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(), base::BindRepeating(&MockMediaRouter::Create));
  }

  void CreateMediaRouterUIForURL(const GURL& url) {
    web_contents()->GetController().LoadURL(url, content::Referrer(),
                                            ui::PAGE_TRANSITION_LINK, "");
    content::RenderFrameHostTester::CommitPendingLoad(
        &web_contents()->GetController());
    CreateSessionServiceTabHelper(web_contents());
    ui_ =
        MediaRouterUI::CreateWithDefaultMediaSourceAndMirroring(web_contents());
  }

  // These methods are used so that we don't have to friend each test case that
  // calls the private methods.
  void NotifyUiOnSinksUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) {
    ui_->OnSinksUpdated(sinks);
  }
  void NotifyUiOnRoutesUpdated(const std::vector<MediaRoute>& routes) {
    ui_->OnRoutesUpdated(routes);
  }

  void StartTabCasting() {
    MediaSource media_source = MediaSource::ForTab(
        sessions::SessionTabHelper::IdForTab(web_contents()).id());
    MediaRouteResponseCallback callback;
    EXPECT_CALL(*mock_router_,
                CreateRouteInternal(media_source.id(), kSinkId, _,
                                    web_contents(), _, base::Seconds(60)))
        .WillOnce(SaveArgWithMove<4>(&callback));
    MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
    for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
      sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
    ui_->StartCasting(kSinkId, MediaCastMode::TAB_MIRROR);
    Mock::VerifyAndClearExpectations(mock_router_);

    NiceMock<MockControllerObserver> observer(ui_.get());
    EXPECT_CALL(observer, OnCastingStarted());
    std::string presentation_id = "presentationId";
    MediaRoute::Id route_id =
        MediaRoute::GetMediaRouteId(presentation_id, kSinkId, media_source);
    MediaRoute route(route_id, media_source, kSinkId,
                     /* description */ std::string(),
                     /* is_local */ true);
    std::unique_ptr<RouteRequestResult> result =
        RouteRequestResult::FromSuccess(route, presentation_id);
    std::move(callback).Run(/* connection */ nullptr, *result);
  }

  void StartCastingAndExpectTimeout(MediaCastMode cast_mode,
                                    const std::string& expected_issue_title,
                                    int timeout_seconds) {
    NiceMock<MockControllerObserver> observer(ui_.get());
    MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
    ui_->OnSinksUpdated({{sink, {cast_mode}}});
    MediaRouteResponseCallback callback;
    EXPECT_CALL(
        *mock_router_,
        CreateRouteInternal(_, _, _, _, _, base::Seconds(timeout_seconds)))
        .WillOnce(SaveArgWithMove<4>(&callback));
    for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
      sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
    ui_->StartCasting(kSinkId, cast_mode);
    Mock::VerifyAndClearExpectations(mock_router_);

    EXPECT_CALL(observer, OnModelUpdated(_))
        .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
          EXPECT_EQ(model.media_sinks()[0].issue->info().title,
                    expected_issue_title);
        }));
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Timed out", mojom::RouteRequestResultCode::TIMED_OUT);
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
    start_presentation_context_ = std::make_unique<StartPresentationContext>(
        presentation_request_,
        base::BindOnce(&PresentationRequestCallbacks::Success,
                       base::Unretained(request_callbacks.get())),
        base::BindOnce(&PresentationRequestCallbacks::Error,
                       base::Unretained(request_callbacks.get())));
    StartPresentationContext* context_ptr = start_presentation_context_.get();
    ui_->media_route_starter()->set_start_presentation_context_for_test(
        std::move(start_presentation_context_));
    ui_->media_route_starter()->OnDefaultPresentationChanged(
        &context_ptr->presentation_request());
    return request_callbacks;
  }

 protected:
  std::vector<raw_ptr<MediaSinksObserver, VectorExperimental>>
      media_sinks_observers_;
  raw_ptr<MockMediaRouter, DanglingUntriaged> mock_router_ = nullptr;
  std::unique_ptr<MediaRouterUI> ui_;
  std::unique_ptr<StartPresentationContext> start_presentation_context_;
  std::unique_ptr<LoggerImpl> logger_;
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

  MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
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
  NotifyUiOnSinksUpdated({sink_with_cast_modes});

  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true);
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(
          WithArg<0>(Invoke([&sink, &route](const CastDialogModel& model) {
            EXPECT_EQ(1u, model.media_sinks().size());
            const UIMediaSink& ui_sink = model.media_sinks()[0];
            EXPECT_EQ(sink.id(), ui_sink.id);
            EXPECT_EQ(UIMediaSinkState::CONNECTED, ui_sink.state);
            EXPECT_EQ(route.media_route_id(), ui_sink.route->media_route_id());
          })));
  NotifyUiOnRoutesUpdated({route});

  EXPECT_CALL(observer, OnControllerDestroyingInternal());
  ui_.reset();
}

TEST_F(MediaRouterViewsUITest, SinkFriendlyName) {
  NiceMock<MockControllerObserver> observer(ui_.get());

  MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
  MediaSinkWithCastModes sink_with_cast_modes(sink);
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(Invoke([&](const CastDialogModel& model) {
        EXPECT_EQ(base::UTF8ToUTF16(sink.name()),
                  model.media_sinks()[0].friendly_name);
      }));
  NotifyUiOnSinksUpdated({sink_with_cast_modes});
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

  // The observer is called multiple times when the default PresentationRequest
  // is changed; the last invocation has the correct header.
  std::u16string current_header;
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillRepeatedly([&](const CastDialogModel& model) {
        current_header = model.dialog_header();
      });

  // First test a presentation started from an https: origin.
  const GURL presentation_url("https://presentation.com");
  const auto https_origin =
      url::Origin::Create(GURL("https://requesting-page.com"));
  // An https origin is included in the dialog header without the scheme.
  content::PresentationRequest presentation_request(
      content::GlobalRenderFrameHostId(), {presentation_url}, https_origin);
  ui_->media_route_starter()->OnDefaultPresentationChanged(
      &presentation_request);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_PRESENTATION_CAST_MODE,
                                       base::UTF8ToUTF16(https_origin.host())),
            current_header);

  // An opaque origin is empty, which causes the dialog to fall back to the tab
  // mirroring header.
  presentation_request = content::PresentationRequest(
      content::GlobalRenderFrameHostId(), {presentation_url}, url::Origin());
  ui_->media_route_starter()->OnDefaultPresentationChanged(
      &presentation_request);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE),
            current_header);

  // An extension origin is replaced by the extension name.
  const std::string extension_id = "extensionid";
  const auto extension_origin =
      url::Origin::Create(GURL("chrome-extension://" + extension_id));
  auto* registry = extensions::ExtensionRegistry::Get(GetBrowserContext());
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(
          "Test Extension", extensions::ExtensionBuilder::Type::EXTENSION)
          .SetID(extension_id)
          .Build();

  ASSERT_TRUE(registry->AddEnabled(extension));

  presentation_request = content::PresentationRequest(
      content::GlobalRenderFrameHostId(), {presentation_url}, extension_origin);
  ui_->media_route_starter()->OnDefaultPresentationChanged(
      &presentation_request);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_PRESENTATION_CAST_MODE,
                                       std::u16string(u"Test Extension")),
            current_header);

  ui_->RemoveObserver(&observer);
}

TEST_F(MediaRouterViewsUITest, StartCasting) {
  StartTabCasting();
}

TEST_F(MediaRouterViewsUITest, StopCasting) {
  EXPECT_CALL(*mock_router_, TerminateRoute(kRouteId));
  ui_->StopCasting(kRouteId);
}

TEST_F(MediaRouterViewsUITest, ConnectingState) {
  NiceMock<MockControllerObserver> observer(ui_.get());

  MediaSink sink{CreateDialSink(kSinkId, kSinkName)};
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
  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true);
  NotifyUiOnRoutesUpdated({route});
}

TEST_F(MediaRouterViewsUITest, DisconnectingState) {
  NiceMock<MockControllerObserver> observer(ui_.get());

  MediaSink sink{CreateDialSink(kSinkId, kSinkName)};
  MediaRoute route(kRouteId, MediaSource(kSourceId), kSinkId, "", true);
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
    sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());
  NotifyUiOnRoutesUpdated({route});

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
  NotifyUiOnRoutesUpdated({});
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(MediaRouterViewsUITest, AddAndRemoveIssue) {
  MediaSink sink1{CreateCastSink("sink_id1", "Sink 1")};
  MediaSink sink2{CreateCastSink("sink_id2", "Sink 2")};
  NotifyUiOnSinksUpdated({{sink1, {MediaCastMode::TAB_MIRROR}},
                          {sink2, {MediaCastMode::TAB_MIRROR}}});

  NiceMock<MockControllerObserver> observer(ui_.get());
  NiceMock<MockIssuesObserver> issues_observer(mock_router_->GetIssueManager());
  issues_observer.Init();
  const std::string issue_title("Issue 1");
  IssueInfo issue(issue_title, IssueInfo::Severity::WARNING, sink2.id());
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
  mock_router_->GetIssueManager()->AddIssue(issue);

  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([&sink2](const CastDialogModel& model) {
        EXPECT_EQ(2u, model.media_sinks().size());
        EXPECT_EQ(model.media_sinks()[1].id, sink2.id());
        EXPECT_FALSE(model.media_sinks()[1].issue.has_value());
      })));
  mock_router_->GetIssueManager()->ClearIssue(issue_id);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(MediaRouterViewsUITest, RouteCreationTimeout) {
  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://presentationurl.com")},
      url::Origin::Create(GURL("https://frameurl.fakeurl")));
  ui_->media_route_starter()->OnDefaultPresentationChanged(
      &presentation_request);
  StartCastingAndExpectTimeout(
      MediaCastMode::PRESENTATION,
      l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_WITH_HOSTNAME,
          u"frameurl.fakeurl"),
      20);
}

TEST_F(MediaRouterViewsUITest, RouteCreationTimeoutIssueTitle) {
  NiceMock<MockIssuesObserver> issues_observer(mock_router_->GetIssueManager());
  issues_observer.Init();

  EXPECT_CALL(issues_observer, OnIssue).WillOnce(Invoke([](const Issue& issue) {
    EXPECT_EQ(l10n_util::GetStringFUTF8(
                  IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_WITH_HOSTNAME,
                  u"presentation_source_name"),
              issue.info().title);
  }));
  ui_->SendIssueForRouteTimeout(MediaCastMode::PRESENTATION, "sink_id",
                                u"presentation_source_name");
  mock_router_->GetIssueManager()->ClearAllIssues();

  EXPECT_CALL(issues_observer, OnIssue).WillOnce(Invoke([](const Issue& issue) {
    EXPECT_EQ(l10n_util::GetStringUTF8(
                  IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB),
              issue.info().title);
  }));
  ui_->SendIssueForRouteTimeout(MediaCastMode::TAB_MIRROR, "sink_id", u"");
  mock_router_->GetIssueManager()->ClearAllIssues();

  EXPECT_CALL(issues_observer, OnIssue).WillOnce(Invoke([](const Issue& issue) {
    EXPECT_EQ(l10n_util::GetStringUTF8(
                  IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_DESKTOP),
              issue.info().title);
  }));
  ui_->SendIssueForRouteTimeout(MediaCastMode::DESKTOP_MIRROR, "sink_id", u"");
  mock_router_->GetIssueManager()->ClearAllIssues();

  EXPECT_CALL(issues_observer, OnIssue).WillOnce(Invoke([](const Issue& issue) {
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT),
        issue.info().title);
  }));
  ui_->SendIssueForRouteTimeout(MediaCastMode::REMOTE_PLAYBACK, "sink_id", u"");
}

#if BUILDFLAG(IS_MAC)
TEST_F(MediaRouterViewsUITest, DesktopMirroringFailsWhenDisallowedOnMac) {
  set_screen_capture_allowed_for_testing(false);
  MockControllerObserver observer(ui_.get());
  MediaSink sink{CreateCastSink(kSinkId, kSinkName)};
  ui_->OnSinksUpdated({{sink, {MediaCastMode::DESKTOP_MIRROR}}});
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_)
    sinks_observer->OnSinksUpdated({sink}, std::vector<url::Origin>());

  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
        EXPECT_EQ(
            model.media_sinks()[0].issue->info().title,
            l10n_util::GetStringUTF8(
                IDS_MEDIA_ROUTER_ISSUE_MAC_SCREEN_CAPTURE_PERMISSION_ERROR));
      }));
  ui_->StartCasting(kSinkId, MediaCastMode::DESKTOP_MIRROR);
}

#endif

TEST_F(MediaRouterViewsUITest, PermissionRejectedIssue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media_router::kShowCastPermissionRejectedError);

  MockControllerObserver observer(ui_.get());
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        EXPECT_TRUE(model.is_permission_rejected());
        EXPECT_TRUE(model.media_sinks().empty());
      })));
  mock_router_->GetIssueManager()->AddPermissionRejectedIssue();
}

TEST_F(MediaRouterViewsUITest, SortedSinks) {
  NotifyUiOnSinksUpdated({{CreateCastSink("sink3", "B sink"), {}},
                          {CreateCastSink("sink2", "A sink"), {}},
                          {CreateCastSink("sink1", "B sink"), {}}});

  // Sort first by name, then by ID.
  const auto& sorted_sinks = ui_->GetEnabledSinks();
  EXPECT_EQ("sink2", sorted_sinks[0].sink.id());
  EXPECT_EQ("sink1", sorted_sinks[1].sink.id());
  EXPECT_EQ("sink3", sorted_sinks[2].sink.id());
}

TEST_F(MediaRouterViewsUITest, SortSinksByIconType) {
  NotifyUiOnSinksUpdated(
      {{MediaSink{"id1", "B sink", SinkIconType::CAST_AUDIO_GROUP,
                  mojom::MediaRouteProviderId::CAST},
        {}},
       {MediaSink{"id2", "sink", SinkIconType::GENERIC,
                  mojom::MediaRouteProviderId::WIRED_DISPLAY},
        {}},
       {MediaSink{"id3", "A sink", SinkIconType::CAST_AUDIO_GROUP,
                  mojom::MediaRouteProviderId::CAST},
        {}},
       {MediaSink{"id4", "sink", SinkIconType::CAST_AUDIO,
                  mojom::MediaRouteProviderId::CAST},
        {}},
       {MediaSink{"id5", "sink", SinkIconType::CAST,
                  mojom::MediaRouteProviderId::CAST},
        {}}});

  // The sorted order is CAST, CAST_AUDIO_GROUP "A", CAST_AUDIO_GROUP "B",
  // CAST_AUDIO, HANGOUT, GENERIC.
  const auto& sorted_sinks = ui_->GetEnabledSinks();
  EXPECT_EQ("id5", sorted_sinks[0].sink.id());
  EXPECT_EQ("id3", sorted_sinks[1].sink.id());
  EXPECT_EQ("id1", sorted_sinks[2].sink.id());
  EXPECT_EQ("id4", sorted_sinks[3].sink.id());
  EXPECT_EQ("id2", sorted_sinks[4].sink.id());
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
  std::vector<MediaSink> sinks = {CreateDialSink(kSinkId, kSinkName)};
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
  std::vector<MediaSink> sinks = {CreateDialSink(kSinkId, kSinkName)};
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
  NiceMock<MockControllerObserver> observer(ui_.get());
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

  NotifyUiOnSinksUpdated(
      {{CreateWiredDisplaySink(display_sink_id1, "sink"), {}},
       {CreateWiredDisplaySink(display_sink_id2, "sink"), {}},
       {CreateDialSink("id3", "sink"), {}}});

  // Initially |display_sink_id1| should not be on the sinks list because we are
  // on |display1|.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
        const auto& sinks = model.media_sinks();
        EXPECT_EQ(2u, sinks.size());
        EXPECT_FALSE(base::Contains(sinks, display_sink_id1, &UIMediaSink::id));
      }));
  ui_->UpdateSinks();
  Mock::VerifyAndClearExpectations(&observer);

  // Change the display to |display2|. Now |display_sink_id2| should be removed
  // from the list of sinks.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>([&](const CastDialogModel& model) {
        const auto& sinks = model.media_sinks();
        EXPECT_EQ(2u, sinks.size());
        EXPECT_FALSE(base::Contains(sinks, display_sink_id2, &UIMediaSink::id));
      }));
  display_observer->set_display(display2);
  ui_->UpdateSinks();
}

TEST_F(MediaRouterViewsUITest, FreezeRoute) {
  EXPECT_CALL(*mock_router_, GetMirroringMediaControllerHost(kRouteId));
  ui_->FreezeRoute(kRouteId);
}

TEST_F(MediaRouterViewsUITest, UnfreezeRoute) {
  EXPECT_CALL(*mock_router_, GetMirroringMediaControllerHost(kRouteId));
  ui_->UnfreezeRoute(kRouteId);
}

TEST_F(MediaRouterViewsUITest, OnFreezeInfoChanged) {
  MockControllerObserver observer;

  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        EXPECT_TRUE(model.media_sinks().empty());
      })));
  ui_->AddObserver(&observer);

  // Calling OnFreezeInfoChanged will trigger the UI to UpdateSinks, which we
  // can detect through OnModelUpdated.
  EXPECT_CALL(observer, OnModelUpdated(_))
      .WillOnce(WithArg<0>(Invoke([](const CastDialogModel& model) {
        EXPECT_TRUE(model.media_sinks().empty());
      })));
  ui_->OnFreezeInfoChanged();

  EXPECT_CALL(observer, OnControllerDestroyingInternal());
  ui_.reset();
}

}  // namespace media_router
