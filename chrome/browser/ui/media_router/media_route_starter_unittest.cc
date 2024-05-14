// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_route_starter.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/browser/test/test_helper.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/route_request_result.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/presentation_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "ui/base/cocoa/permissions_utils.h"
#endif

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace media_router {

namespace {
constexpr char kPresentationId[] = "session-67890";
constexpr char kLoggerComponent[] = "MediaRouteStarterTests";

const CastModeSet kDefaultModes = {MediaCastMode::PRESENTATION,
                                   MediaCastMode::TAB_MIRROR,
                                   MediaCastMode::DESKTOP_MIRROR};
const CastModeSet kMirroringOnly = {MediaCastMode::TAB_MIRROR,
                                    MediaCastMode::DESKTOP_MIRROR};
const CastModeSet kPresentationOnly = {MediaCastMode::PRESENTATION};
const CastModeSet kDestkopMirrorOnly = {MediaCastMode::DESKTOP_MIRROR};
const CastModeSet kRemotePlaybackOnly = {MediaCastMode::REMOTE_PLAYBACK};

constexpr char kDefaultPresentationUrl[] = "https://defaultpresentation.com/";
constexpr char kDefaultOriginUrl[] = "https://default.fakeurl/";

constexpr char kStartPresentationUrl[] = "https://startpresentrequest.com/";
constexpr char kStartOriginUrl[] = "https://start.fakeurl/";

constexpr char kRemotePlaybackUrl[] =
    "remote-playback:media-element?source=encoded_data&video_codec=vp8&audio_"
    "codec=mp3";

class MockPresentationRequestSourceObserver
    : public PresentationRequestSourceObserver {
 public:
  MockPresentationRequestSourceObserver() = default;
  explicit MockPresentationRequestSourceObserver(MediaRouteStarter* starter)
      : starter_(starter) {
    starter_->AddPresentationRequestSourceObserver(this);
  }

  ~MockPresentationRequestSourceObserver() override {
    if (starter_)
      starter_->RemovePresentationRequestSourceObserver(this);
  }

  MOCK_METHOD(void, OnSourceUpdated, (std::u16string&));

 private:
  raw_ptr<MediaRouteStarter> starter_ = nullptr;
};

// For demonstrating that presentation mode callbacks are made from the MRS
// d'tor
class PresentationRequestCallbacks {
 public:
  PresentationRequestCallbacks() = default;

  explicit PresentationRequestCallbacks(
      const blink::mojom::PresentationError& expected_error)
      : expected_error_(expected_error) {}
  ~PresentationRequestCallbacks() { EXPECT_TRUE(called_); }

  void Success(const blink::mojom::PresentationInfo&,
               mojom::RoutePresentationConnectionPtr,
               const MediaRoute&) {
    NOTREACHED_IN_MIGRATION();
  }

  void Error(const blink::mojom::PresentationError& error) {
    EXPECT_EQ(expected_error_.error_type, error.error_type);
    EXPECT_EQ(expected_error_.message, error.message);
    called_ = true;
  }

 private:
  blink::mojom::PresentationError expected_error_;
  bool called_ = false;
};

}  // namespace

class MediaRouteStarterTest : public ChromeRenderViewHostTestHarness {
 public:
  // For demonstrating that presentation mode casting callbacks are called.
  MOCK_METHOD(void,
              RequestSuccess,
              (const blink::mojom::PresentationInfo&,
               mojom::RoutePresentationConnectionPtr,
               const MediaRoute&));
  MOCK_METHOD(void,
              RequestError,
              (const blink::mojom::PresentationError& error));

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    presentation_manager_ =
        std::make_unique<NiceMock<MockWebContentsPresentationManager>>();
    WebContentsPresentationManager::SetTestInstance(presentation_manager());

    SetMediaRouterFactory();
    logger_ = std::make_unique<LoggerImpl>();

    CreateSessionServiceTabHelper(web_contents());

    cast_sink_ = CreateCastSink(1);
    dial_sink_ = CreateDialSink(1);

    CreateStarterForDefaultModes();
  }

  void TearDown() override {
    clear_screen_capture_allowed_for_testing();
    DestroyMediaRouteStarter();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    WebContentsPresentationManager::SetTestInstance(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual void SetMediaRouterFactory() {
    ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(), base::BindRepeating(&MockMediaRouter::Create));
    Profile* default_profile =
#if BUILDFLAG(IS_CHROMEOS)
        ProfileManager::GetActiveUserProfile();
#else
        ProfileManager::GetLastUsedProfile();
#endif  // BUILDFLAG(IS_CHROMEOS)

    ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        default_profile, base::BindRepeating(&MockMediaRouter::Create));
  }

  void CreateStarterForDefaultModes() {
    CreateStarter(MediaRouterUIParameters(kDefaultModes, web_contents()));
  }

  void CreateStarter(MediaRouterUIParameters params) {
    starter_ = std::make_unique<MediaRouteStarter>(std::move(params));
    starter_->SetLoggerComponent(kLoggerComponent);
    ON_CALL(*media_router(), GetLogger()).WillByDefault(Return(logger_.get()));
    // Store sink observers so that they can be notified in tests.
    ON_CALL(*media_router(), RegisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          media_sinks_observers_.push_back(observer);
          return true;
        });
    // Remove sink observers as appropriate (destructing handlers will cause
    // this to occur).
    ON_CALL(*media_router(), UnregisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          auto it = base::ranges::find(media_sinks_observers_, observer);
          if (it != media_sinks_observers_.end()) {
            media_sinks_observers_.erase(it);
          }
        });
    // Handler so MockMediaRouter will respond to requests to create a route.
    // Will construct a RouteRequestResult based on the set result code and
    // then call the handler's callback.
    ON_CALL(*media_router(), CreateRouteInternal(_, _, _, _, _, _))
        .WillByDefault([this](const MediaSource::Id& source_id,
                              const MediaSink::Id& sink_id,
                              const url::Origin& origin,
                              content::WebContents* web_contents,
                              MediaRouteResponseCallback& callback,
                              base::TimeDelta timeout) {
          // This indicates the test did not properly set the expected result
          EXPECT_NE(mojom::RouteRequestResultCode::UNKNOWN_ERROR, result_code_);
          std::unique_ptr<RouteRequestResult> result;
          if (result_code_ == mojom::RouteRequestResultCode::OK) {
            result = GetSuccessResult(source_id, sink_id);
          } else {
            result = GetErrorResult(result_code_);
          }
          std::move(callback).Run(nullptr, *result);
        });
  }

  MediaRouteStarter* media_route_starter() { return starter_.get(); }
  TestingProfileManager* profile_manager() { return profile_manager_.get(); }
  MockMediaRouter* media_router() {
    return static_cast<MockMediaRouter*>(
        media_route_starter()->GetMediaRouter());
  }
  LoggerImpl* logger() { return logger_.get(); }

  const std::vector<raw_ptr<MediaSinksObserver, VectorExperimental>>
  media_sink_observers() {
    return media_sinks_observers_;
  }

  MockWebContentsPresentationManager* presentation_manager() {
    return presentation_manager_.get();
  }

  QueryResultManager* query_result_manager() {
    return media_route_starter()->GetQueryResultManager();
  }

  const RouteRequestResult* route_request_result() {
    return route_request_result_.get();
  }

  const MediaSinkInternal& cast_sink() { return cast_sink_; }
  const MediaSinkInternal& dial_sink() { return dial_sink_; }

  const content::PresentationRequest& default_presentation_request() {
    return default_presentation_request_;
  }
  const content::PresentationRequest& start_presentation_request() {
    return start_presentation_request_;
  }

  void set_expected_cast_result(mojom::RouteRequestResultCode code) {
    result_code_ = code;
  }

  std::unique_ptr<StartPresentationContext> CreateStartPresentationContext(
      const content::PresentationRequest& presentation_request) {
    return std::make_unique<StartPresentationContext>(
        presentation_request,
        base::BindOnce(&MediaRouteStarterTest::RequestSuccess,
                       base::Unretained(this)),
        base::BindOnce(&MediaRouteStarterTest::RequestError,
                       base::Unretained(this)));
  }

  // The caller must hold on to PresentationRequestCallbacks returned so that
  // a callback can later be called on it.
  std::unique_ptr<PresentationRequestCallbacks> ExpectPresentationError(
      const content::PresentationRequest& presentation_request,
      blink::mojom::PresentationErrorType error_type,
      const std::string& error_message) {
    blink::mojom::PresentationError expected_error(error_type, error_message);
    auto request_callbacks =
        std::make_unique<PresentationRequestCallbacks>(expected_error);
    auto start_presentation_context =
        std::make_unique<StartPresentationContext>(
            presentation_request,
            base::BindOnce(&PresentationRequestCallbacks::Success,
                           base::Unretained(request_callbacks.get())),
            base::BindOnce(&PresentationRequestCallbacks::Error,
                           base::Unretained(request_callbacks.get())));
    CreateStarter(
        MediaRouterUIParameters(kPresentationOnly, web_contents(),
                                std::move(start_presentation_context)));
    return request_callbacks;
  }

  void UpdateSinks(const std::vector<MediaSink>& sinks,
                   const std::vector<url::Origin>& origins) {
    for (MediaSinksObserver* sinks_observer : media_sink_observers()) {
      sinks_observer->OnSinksUpdated(sinks, origins);
    }
  }

  std::string GetLogEntry(const std::string& logs_json,
                          const std::string& attribute) {
    base::Value logs = base::JSONReader::Read(logs_json).value();
    return *logs.GetList()[0].GetDict().FindString(attribute);
  }

  void DestroyMediaRouteStarter() { starter_.reset(); }

  std::unique_ptr<RouteRequestResult> GetSuccessResult(
      const MediaSource::Id& source_id,
      const MediaSink::Id& sink_id) {
    MediaSource source(source_id);
    MediaRoute route;
    route.set_media_route_id(source_id + "->" + sink_id);
    route.set_media_source(source);
    route.set_media_sink_id(sink_id);
    return RouteRequestResult::FromSuccess(
        route, IsValidPresentationUrl(source.url()) ? kPresentationId : "");
  }

  std::unique_ptr<RouteRequestResult> GetSuccessResult(
      const content::PresentationRequest& request,
      const MediaSink::Id& sink_id) {
    auto source =
        MediaSource::ForPresentationUrl(*(request.presentation_urls.begin()))
            .id();
    return GetSuccessResult(source, sink_id);
  }

  std::unique_ptr<RouteRequestResult> GetErrorResult(
      mojom::RouteRequestResultCode result_code) {
    return RouteRequestResult::FromError("unit test error", result_code);
  }

  void StartMirroring(const MediaSinkInternal& sink, MediaCastMode cast_mode) {
    EXPECT_NE(MediaCastMode::PRESENTATION, cast_mode);

    // Add a sink
    UpdateSinks({sink.sink()}, std::vector<url::Origin>());

    auto params =
        media_route_starter()->CreateRouteParameters(sink.id(), cast_mode);
    EXPECT_TRUE(params);

    if (cast_mode == MediaCastMode::DESKTOP_MIRROR)
      set_screen_capture_allowed_for_testing(true);

    StartRoute(std::move(params));
  }

  void StartPresentation(const MediaSinkInternal& sink,
                         const content::PresentationRequest& request) {
    // Add a presentation compatible sink
    UpdateSinks({sink.sink()}, {request.frame_origin});

    auto params = media_route_starter()->CreateRouteParameters(
        sink.id(), MediaCastMode::PRESENTATION);

    EXPECT_TRUE(params);

    StartRoute(std::move(params));
  }

  void StartRemotePlayback(const MediaSinkInternal& sink) {
    UpdateSinks({sink.sink()}, std::vector<url::Origin>());

    auto params = media_route_starter()->CreateRouteParameters(
        sink.id(), MediaCastMode::REMOTE_PLAYBACK);
    EXPECT_TRUE(params);
    StartRoute(std::move(params));
  }

  content::PresentationRequest CreatePresentationRequest(
      const std::string& presentation_url,
      const std::string& origin_url) {
    content::PresentationRequest presentation_request(
        {0, 0}, {GURL(presentation_url)},
        url::Origin::Create(GURL(origin_url)));
    return presentation_request;
  }

 private:
  void StartRoute(std::unique_ptr<RouteParameters> params) {
    // To demonstrate that MediaRouteResultCallbacks are called
    params->route_result_callbacks.emplace_back(
        base::BindOnce(&MediaRouteStarterTest::HandleMediaRouteResponse,
                       base::Unretained(this)));

    EXPECT_CALL(*media_router(),
                CreateRouteInternal(params->source_id, params->request->sink_id,
                                    params->origin, web_contents(), _,
                                    params->timeout));

    media_route_starter()->StartRoute(std::move(params));
  }

  // For demonstrating that the MediaRouteResultCallbacks are called.
  void HandleMediaRouteResponse(const RouteRequestResult& result) {
    // Store the response so tests can examine it.
    if (result.result_code() == mojom::RouteRequestResultCode::OK) {
      auto route = std::make_unique<MediaRoute>(*result.route());
      route_request_result_ = std::make_unique<RouteRequestResult>(
          std::move(route), result.presentation_id(), result.error(),
          result.result_code());
    } else {
      route_request_result_ =
          RouteRequestResult::FromError(result.error(), result.result_code());
    }
  }

  std::unique_ptr<MediaRouteStarter> starter_;

  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::unique_ptr<LoggerImpl> logger_;
  std::vector<raw_ptr<MediaSinksObserver, VectorExperimental>>
      media_sinks_observers_;

  std::unique_ptr<MockWebContentsPresentationManager> presentation_manager_;

  MediaSinkInternal cast_sink_;
  MediaSinkInternal dial_sink_;

  content::PresentationRequest default_presentation_request_ =
      CreatePresentationRequest(kDefaultPresentationUrl, kDefaultOriginUrl);
  content::PresentationRequest start_presentation_request_ =
      CreatePresentationRequest(kStartPresentationUrl, kStartOriginUrl);

  mojom::RouteRequestResultCode result_code_ =
      mojom::RouteRequestResultCode::UNKNOWN_ERROR;
  std::unique_ptr<RouteRequestResult> route_request_result_;
};

// Demonstrates that when initialized with webcontents but no presentation
// source the supported modes are mirroring only.
TEST_F(MediaRouteStarterTest, Defaults_NoPresentation) {
  CreateStarterForDefaultModes();
  EXPECT_EQ(kMirroringOnly, query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that when initialized with webcontents that has a default
// presentation that presentation mode is supported.
TEST_F(MediaRouteStarterTest, Defaults_WebContentPresentation) {
  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  CreateStarterForDefaultModes();
  EXPECT_EQ(kDefaultModes, query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that if caller doesn't request mirroring that it is not
// available.
TEST_F(MediaRouteStarterTest, Defaults_WebContentPresentationOnly) {
  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  CreateStarter(MediaRouterUIParameters(kPresentationOnly, web_contents()));
  EXPECT_EQ(kPresentationOnly, query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that when initialized with a presentation request that
// presentation mode is supported - even if the web contents has no default
// presentation.
TEST_F(MediaRouteStarterTest, Defaults_StartPresentationContext) {
  auto start_presentation_context =
      CreateStartPresentationContext(start_presentation_request());

  CreateStarter(MediaRouterUIParameters(kDefaultModes, web_contents(),
                                        std::move(start_presentation_context)));
  EXPECT_EQ(kDefaultModes, query_result_manager()->GetSupportedCastModes());

  // This is to deal with the error callback in the d'tor that's not part of
  // this test. See the Dtor_* tests below where this case is actually
  // validated.
  EXPECT_CALL(*this, RequestError(_));
}

// Demonstrates that when initialized with Remote Playback mode
TEST_F(MediaRouteStarterTest, Defaults_RemotePlayback) {
  CreateStarter(MediaRouterUIParameters(kRemotePlaybackOnly, web_contents(),
                                        nullptr, media::VideoCodec::kVP8,
                                        media::AudioCodec::kOpus));
  EXPECT_EQ(kRemotePlaybackOnly,
            query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that when initialized with no webcontent or presentation source
// the supported modes are desktop mirroring only.
TEST_F(MediaRouteStarterTest, Defaults_NoWebContent) {
  CreateStarter(MediaRouterUIParameters(kDefaultModes, nullptr));
  EXPECT_EQ(kDestkopMirrorOnly,
            query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that when MediaRouteStarter is notified that the presentation
// request source has changed, that it alerts observers with the name of that
// presentation request source.
TEST_F(MediaRouteStarterTest, OnPresentationRequestSourceUpdated) {
  CreateStarterForDefaultModes();
  EXPECT_EQ(kMirroringOnly, query_result_manager()->GetSupportedCastModes());

  MockPresentationRequestSourceObserver observer(media_route_starter());

  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  std::u16string expected_name(u"default.fakeurl");
  EXPECT_CALL(observer, OnSourceUpdated(expected_name));

  // Simulate the notification that the default presentation has changed.
  media_route_starter()->OnDefaultPresentationChanged(
      &presentation_manager()->GetDefaultPresentationRequest());

  // Now that a default presentation has been added the available modes should
  // include presentation.
  EXPECT_EQ(kDefaultModes, query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that when MediaRouteStarter is notified that there is no
// presentation request source that it alerts observers that the name of the
// presentation source is empty.
TEST_F(MediaRouteStarterTest, OnPresentationRequestSourceRemoved) {
  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  CreateStarterForDefaultModes();
  EXPECT_EQ(kDefaultModes, query_result_manager()->GetSupportedCastModes());

  MockPresentationRequestSourceObserver observer(media_route_starter());

  // Simulate the notification that the default presentation has been removed.
  std::u16string expected_name(u"");
  EXPECT_CALL(observer, OnSourceUpdated(expected_name));

  media_route_starter()->OnDefaultPresentationChanged(nullptr);

  // Now that a default presentation has been added the available modes should
  // include presentation.
  EXPECT_EQ(kMirroringOnly, query_result_manager()->GetSupportedCastModes());
}

// Demonstrates that if MediaRouteStarter is destroyed without an attempt to
// create a route with the presentation source, that the expected error is
// reported to the presentation source.
TEST_F(MediaRouteStarterTest, Dtor_NotFoundError_NoSinks) {
  auto request_callbacks = ExpectPresentationError(
      default_presentation_request(),
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");

  // Destroying the starter should return the expected error from above to the
  // error callback.
  DestroyMediaRouteStarter();
}

// Same as above, but demonstrates the same error reporting even if sinks exist,
// so long as none of the sinks were compatible with the presentation source.
TEST_F(MediaRouteStarterTest, Dtor_NotFoundError_NoCompatibleSinks) {
  auto request_callbacks = ExpectPresentationError(
      default_presentation_request(),
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found.");
  // Send a sink to the UI that is compatible with sources other than the
  // presentation url to cause a NotFoundError.
  std::vector<MediaSink> sinks = {dial_sink().sink()};
  auto presentation_source = MediaSource::ForPresentationUrl(
      default_presentation_request().presentation_urls[0]);
  for (MediaSinksObserver* sinks_observer : media_sink_observers()) {
    if (!(sinks_observer->source() == presentation_source)) {
      sinks_observer->OnSinksUpdated(sinks, {});
    }
  }

  // Destroying the starter should return the expected error from above to the
  // error callback.
  DestroyMediaRouteStarter();
}

// Same as above, but demonstrates that if a compatible sink was present, then
// the error that is reported indicates that the request was cancelled.
TEST_F(MediaRouteStarterTest, Dtor_AbortError) {
  auto request_callbacks = ExpectPresentationError(
      default_presentation_request(),
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
      "Dialog closed.");
  // Send a sink to the UI that is compatible with the presentation url to avoid
  // a NotFoundError.
  std::vector<MediaSink> sinks = {dial_sink().sink()};
  auto presentation_source = MediaSource::ForPresentationUrl(
      default_presentation_request().presentation_urls[0]);
  for (MediaSinksObserver* sinks_observer : media_sink_observers()) {
    if (sinks_observer->source() == presentation_source) {
      sinks_observer->OnSinksUpdated(sinks, {});
    }
  }

  // Destroying the starter should return the expected error from above to the
  // error callback.
  DestroyMediaRouteStarter();
}

// Demonstrates that if there are no sources available for the desired mode
// CreateRouteParameters returns nothing.
TEST_F(MediaRouteStarterTest, CreateRouteParameters_NoValidSource) {
  // No presentation available
  CreateStarter(MediaRouterUIParameters(kMirroringOnly, web_contents()));

  // Add a sink
  UpdateSinks({cast_sink().sink()}, std::vector<url::Origin>());

  auto params = media_route_starter()->CreateRouteParameters(
      cast_sink().id(), MediaCastMode::PRESENTATION);

  EXPECT_FALSE(params);
}

// Demonstrates that when desktop mirroring is available and requested that the
// RouteParameters are properly filled out.
TEST_F(MediaRouteStarterTest, CreateRouteParameters_DesktopMirroring) {
  CreateStarterForDefaultModes();

  // Add a sink
  UpdateSinks({cast_sink().sink()}, std::vector<url::Origin>());

  auto params = media_route_starter()->CreateRouteParameters(
      cast_sink().id(), MediaCastMode::DESKTOP_MIRROR);

  EXPECT_EQ(MediaCastMode::DESKTOP_MIRROR, params->cast_mode);
  EXPECT_EQ(MediaSource::ForUnchosenDesktop().id(), params->source_id);
  EXPECT_EQ(cast_sink().id(), params->request->sink_id);
  EXPECT_EQ(GURL(), params->origin.GetURL());
  // route_result_callbacks should only be filled in by caller
  EXPECT_EQ(0ul, params->route_result_callbacks.size());
  EXPECT_EQ(base::Seconds(120), params->timeout);
}

// Demonstrates that when tab mirroring is available and requested that the
// RouteParameters are properly filled out.
TEST_F(MediaRouteStarterTest, CreateRouteParameters_TabMirroring) {
  SessionID::id_type tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents()).id();

  CreateStarterForDefaultModes();

  // Add a sink
  UpdateSinks({cast_sink().sink()}, std::vector<url::Origin>());

  auto params = media_route_starter()->CreateRouteParameters(
      cast_sink().id(), MediaCastMode::TAB_MIRROR);

  EXPECT_EQ(MediaCastMode::TAB_MIRROR, params->cast_mode);
  EXPECT_EQ(MediaSource::ForTab(tab_id).id(), params->source_id);
  EXPECT_EQ(cast_sink().id(), params->request->sink_id);
  EXPECT_EQ(GURL(), params->origin.GetURL());
  // route_result_callbacks should only be filled in by caller
  EXPECT_EQ(0ul, params->route_result_callbacks.size());
  EXPECT_EQ(base::Seconds(60), params->timeout);
}

// Demonstrates that when presentation mode is available for the default
// presentation and requested that the RouteParameters are properly filled out.
TEST_F(MediaRouteStarterTest, CreateRouteParameters_WebContentPresentation) {
  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  CreateStarterForDefaultModes();

  // Add a presentation compatible sink
  UpdateSinks({cast_sink().sink()},
              {default_presentation_request().frame_origin});

  auto params = media_route_starter()->CreateRouteParameters(
      cast_sink().id(), MediaCastMode::PRESENTATION);

  EXPECT_EQ(MediaCastMode::PRESENTATION, params->cast_mode);
  EXPECT_EQ(MediaSource::ForPresentationUrl(
                *(default_presentation_request()).presentation_urls.begin())
                .id(),
            params->source_id);
  EXPECT_EQ(cast_sink().id(), params->request->sink_id);
  EXPECT_EQ(default_presentation_request().frame_origin, params->origin);
  // route_result_callbacks should only be filled in by caller
  EXPECT_EQ(0ul, params->route_result_callbacks.size());
  EXPECT_EQ(base::Seconds(20), params->timeout);
}

// Demonstrates that when presentation mode is requested and a start
// presentation context is available that the RouteParameters are correctly
// filled out.
TEST_F(MediaRouteStarterTest, CreateRouteParameters_StartPresentationContext) {
  auto start_presentation_context =
      CreateStartPresentationContext(start_presentation_request());

  CreateStarter(MediaRouterUIParameters(kDefaultModes, web_contents(),
                                        std::move(start_presentation_context)));

  // Add a presentation compatible sink
  UpdateSinks({cast_sink().sink()},
              {start_presentation_request().frame_origin});

  auto params = media_route_starter()->CreateRouteParameters(
      cast_sink().id(), MediaCastMode::PRESENTATION);

  EXPECT_EQ(MediaCastMode::PRESENTATION, params->cast_mode);
  EXPECT_EQ(MediaSource::ForPresentationUrl(
                *(start_presentation_request().presentation_urls.begin()))
                .id(),
            params->source_id);
  EXPECT_EQ(cast_sink().id(), params->request->sink_id);
  EXPECT_EQ(start_presentation_request().frame_origin, params->origin);
  // route_result_callbacks should only be filled in by caller
  EXPECT_EQ(0ul, params->route_result_callbacks.size());
  EXPECT_EQ(base::Seconds(20), params->timeout);

  // This is to deal with the error callback in the d'tor that's not part of
  // this test. See the Dtor_* tests below where this case is actually
  // validated.
  EXPECT_CALL(*this, RequestError(_));
}

// Demonstrates that desktop mirroring routes are created correctly.
TEST_F(MediaRouteStarterTest, StartRoute_DesktopMirroring) {
  CreateStarterForDefaultModes();

  set_expected_cast_result(mojom::RouteRequestResultCode::OK);

  StartMirroring(cast_sink(), MediaCastMode::DESKTOP_MIRROR);

  EXPECT_EQ(mojom::RouteRequestResultCode::OK,
            route_request_result()->result_code());

  MediaSource expected_source = MediaSource::ForUnchosenDesktop();
  EXPECT_EQ(expected_source, route_request_result()->route()->media_source());
}

// Demonstrates that failures to create desktop mirroring routes are propagated.
TEST_F(MediaRouteStarterTest, StartRoute_DesktopMirroringError) {
  CreateStarterForDefaultModes();

  set_expected_cast_result(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);

  StartMirroring(cast_sink(), MediaCastMode::DESKTOP_MIRROR);

  EXPECT_EQ(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND,
            route_request_result()->result_code());
}

// Demonstrates that tab mirroring routes are created correctly.
TEST_F(MediaRouteStarterTest, StartRoute_TabMirroring) {
  CreateStarterForDefaultModes();

  set_expected_cast_result(mojom::RouteRequestResultCode::OK);

  StartMirroring(cast_sink(), MediaCastMode::TAB_MIRROR);

  EXPECT_EQ(mojom::RouteRequestResultCode::OK,
            route_request_result()->result_code());

  MediaSource expected_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  EXPECT_EQ(expected_source, route_request_result()->route()->media_source());
}

// Demonstrates that failures to create tab mirroring routes are propagated.
TEST_F(MediaRouteStarterTest, StartRoute_TabMirroringError) {
  CreateStarterForDefaultModes();

  set_expected_cast_result(mojom::RouteRequestResultCode::INVALID_ORIGIN);

  StartMirroring(cast_sink(), MediaCastMode::DESKTOP_MIRROR);

  EXPECT_EQ(mojom::RouteRequestResultCode::INVALID_ORIGIN,
            route_request_result()->result_code());
}

// Demonstrates that presentations routes from web content are created
// correctly.
TEST_F(MediaRouteStarterTest, StartRoute_WebContentPresentation) {
  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  CreateStarterForDefaultModes();

  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  auto expected_result =
      GetSuccessResult(default_presentation_request(), cast_sink().id());

  EXPECT_CALL(*presentation_manager(),
              OnPresentationResponse(default_presentation_request(), _, _));

  StartPresentation(cast_sink(), default_presentation_request());

  EXPECT_EQ(mojom::RouteRequestResultCode::OK,
            route_request_result()->result_code());
  EXPECT_EQ(kDefaultPresentationUrl,
            route_request_result()->presentation_url());
}

// Demonstrates that failures to create presentation routes from web content are
// propagated correctly.
TEST_F(MediaRouteStarterTest, StartRoute_WebContentPresentationError) {
  presentation_manager()->SetDefaultPresentationRequest(
      default_presentation_request());

  CreateStarterForDefaultModes();

  set_expected_cast_result(mojom::RouteRequestResultCode::INVALID_ORIGIN);
  EXPECT_CALL(*presentation_manager(),
              OnPresentationResponse(default_presentation_request(), _, _));

  StartPresentation(cast_sink(), default_presentation_request());

  EXPECT_EQ(mojom::RouteRequestResultCode::INVALID_ORIGIN,
            route_request_result()->result_code());
}

// Demonstrates that presentations routes from start presentation contexts are
// created correctly.
TEST_F(MediaRouteStarterTest, StartRoute_StartPresentationContext_Cast) {
  auto start_presentation_context =
      CreateStartPresentationContext(start_presentation_request());

  CreateStarter(MediaRouterUIParameters(kDefaultModes, web_contents(),
                                        std::move(start_presentation_context)));

  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  auto expected_result =
      GetSuccessResult(start_presentation_request(), cast_sink().id());

  EXPECT_CALL(*this, RequestSuccess(_, _, *expected_result->route()));

  StartPresentation(cast_sink(), start_presentation_request());

  EXPECT_EQ(mojom::RouteRequestResultCode::OK,
            route_request_result()->result_code());
  EXPECT_EQ(kStartPresentationUrl, route_request_result()->presentation_url());
}

TEST_F(MediaRouteStarterTest,
       StartRoute_StartPresentationContext_RemotePlayback) {
  auto start_presentation_context = CreateStartPresentationContext(
      CreatePresentationRequest(kRemotePlaybackUrl, kStartOriginUrl));

  CreateStarter(MediaRouterUIParameters(kDefaultModes, web_contents(),
                                        std::move(start_presentation_context)));

  set_expected_cast_result(mojom::RouteRequestResultCode::OK);

  EXPECT_CALL(*this, RequestSuccess(_, _, _));

  StartRemotePlayback(cast_sink());

  // TODO(crbug.com/1491212): Update test case once `tab_id` is removed from the
  // Remote Playback presentation url.
  EXPECT_EQ(mojom::RouteRequestResultCode::OK,
            route_request_result()->result_code());
  EXPECT_EQ(
      base::StrCat(
          {kRemotePlaybackUrl, "&tab_id=",
           base::NumberToString(
               sessions::SessionTabHelper::IdForTab(web_contents()).id())}),
      route_request_result()->presentation_url());
}

// Demonstrates that failures to create presentation routes from start
// presentation contexts are created correctly.
TEST_F(MediaRouteStarterTest, StartRoute_StartPresentationContextError) {
  auto start_presentation_context =
      CreateStartPresentationContext(start_presentation_request());

  CreateStarter(MediaRouterUIParameters(kDefaultModes, web_contents(),
                                        std::move(start_presentation_context)));

  set_expected_cast_result(
      mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER);

  EXPECT_CALL(*this, RequestError(_));

  StartPresentation(cast_sink(), start_presentation_request());

  EXPECT_EQ(mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER,
            route_request_result()->result_code());
}

TEST_F(MediaRouteStarterTest, GetScreenCapturePermission) {
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  bool screen_capture_is_allowed = false;
#else
  bool screen_capture_is_allowed = true;
#endif  // BUILDFLAG(IS_MAC)
  set_screen_capture_allowed_for_testing(true);
  // Always allowed for presentation mode and tab mirroring.
  EXPECT_TRUE(MediaRouteStarter::GetScreenCapturePermission(
      MediaCastMode::PRESENTATION));
  EXPECT_TRUE(
      MediaRouteStarter::GetScreenCapturePermission(MediaCastMode::TAB_MIRROR));
  // Always allowed for desktop mode if permission has been granted
  EXPECT_TRUE(MediaRouteStarter::GetScreenCapturePermission(
      MediaCastMode::DESKTOP_MIRROR));

  set_screen_capture_allowed_for_testing(false);
  // Always allowed for presentation mode and tab mirroring.
  EXPECT_TRUE(MediaRouteStarter::GetScreenCapturePermission(
      MediaCastMode::PRESENTATION));
  EXPECT_TRUE(
      MediaRouteStarter::GetScreenCapturePermission(MediaCastMode::TAB_MIRROR));
  // The question of whether permission needs to be granted depends on platform
  // and version.
  EXPECT_EQ(screen_capture_is_allowed,
            MediaRouteStarter::GetScreenCapturePermission(
                MediaCastMode::DESKTOP_MIRROR));
}

}  // namespace media_router
