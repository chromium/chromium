// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/route_request_result.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using access_code_cast::mojom::AddSinkResultCode;
using MockAddSinkCallback =
    base::MockCallback<media_router::AccessCodeCastHandler::AddSinkCallback>;
using MockCastToSinkCallback =
    base::MockCallback<media_router::AccessCodeCastHandler::CastToSinkCallback>;
using media_router::mojom::RouteRequestResultCode;
using ::testing::_;
using ::testing::Eq;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

namespace {
class MockPage : public access_code_cast::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<access_code_cast::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<access_code_cast::mojom::Page> receiver_{this};
};

const char kEmail[] = "mock_email@gmail.com";
constexpr char histogram[] =
    "AccessCodeCast.Session.NewDeviceRouteCreationDuration";

}  // namespace

class AccessCodeCastHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AccessCodeCastHandlerTest()
      : mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_cast_socket_service_(
            new cast_channel::MockCastSocketService(mock_time_task_runner_)),
        message_handler_(mock_cast_socket_service_.get()),
        mock_cast_media_sink_service_impl_(
            new MockCastMediaSinkServiceImpl(mock_sink_discovered_cb_.Get(),
                                             mock_cast_socket_service_.get(),
                                             discovery_network_monitor_.get(),
                                             &dual_media_sink_service_)) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
    // `identity_test_environment_` starts signed-out while `sync_service_`
    // starts signed-in, make them consistent.
    sync_service_.SetSignedOut();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager()->CreateTestingProfile(kEmail);

    presentation_manager_ =
        std::make_unique<NiceMock<MockWebContentsPresentationManager>>();
    WebContentsPresentationManager::SetTestInstance(
        presentation_manager_.get());

    SetMediaRouterFactory();

    cast_sink_1_ = CreateCastSink(1);
    cast_sink_2_ = CreateCastSink(2);

    CreateSessionServiceTabHelper(web_contents());

    CreateHandler({MediaCastMode::DESKTOP_MIRROR});
  }

  void TearDown() override {
    clear_screen_capture_allowed_for_testing();
    handler_.reset();
    access_code_cast_sink_service_.reset();
    page_.reset();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    WebContentsPresentationManager::SetTestInstance(nullptr);
    task_environment()->RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual void SetMediaRouterFactory() {
    router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            web_contents()->GetBrowserContext(),
            base::BindRepeating(&MockMediaRouter::Create)));
    ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&MockMediaRouter::Create));

    InitializeMockMediaRouter();
  }

  base::TestMockTimeTaskRunner* mock_time_task_runner() {
    return mock_time_task_runner_.get();
  }

  void CreateHandler(const CastModeSet& cast_modes,
                     std::unique_ptr<StartPresentationContext>
                         start_presentation_context = nullptr) {
    page_ = std::make_unique<StrictMock<MockPage>>();

    access_code_cast_sink_service_ =
        std::make_unique<MockAccessCodeCastSinkService>(
            profile_, router_, mock_cast_media_sink_service_impl_.get(),
            discovery_network_monitor_.get());
    access_code_cast_sink_service_->SetTaskRunnerForTesting(
        mock_time_task_runner_);

    std::unique_ptr<MediaRouteStarter> starter =
        std::make_unique<MediaRouteStarter>(MediaRouterUIParameters(
            cast_modes, web_contents(), std::move(start_presentation_context)));

    handler_ = base::WrapUnique(new AccessCodeCastHandler(
        mojo::PendingReceiver<access_code_cast::mojom::PageHandler>(),
        page_->BindAndGetRemote(), cast_modes, std::move(starter),
        access_code_cast_sink_service_.get()));
  }

  AccessCodeCastHandler* handler() { return handler_.get(); }

  MediaRouteStarter* media_route_starter() {
    return handler_->GetMediaRouteStarterForTesting();
  }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  MockWebContentsPresentationManager* presentation_manager() {
    return presentation_manager_.get();
  }

  MockMediaRouter* router() { return router_; }

  MockAccessCodeCastSinkService* access_service() {
    return access_code_cast_sink_service_.get();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  syncer::SyncService& sync_service() { return sync_service_; }

  void set_expected_cast_result(mojom::RouteRequestResultCode code) {
    result_code_ = code;
  }

  void StartDesktopMirroring(const MediaSource& source,
                             MockCastToSinkCallback& mock_callback) {
    StartMirroring({MediaCastMode::DESKTOP_MIRROR}, source, base::Seconds(120),
                   mock_callback);
  }

  void StartTabMirroring(const MediaSource& source,
                         MockCastToSinkCallback& mock_callback) {
    StartMirroring({MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR},
                   source, base::Seconds(60), mock_callback);
  }

  void StartMirroring(const CastModeSet& cast_modes,
                      const MediaSource& source,
                      base::TimeDelta timeout,
                      MockCastToSinkCallback& mock_callback) {
    CreateHandler(cast_modes);
    set_screen_capture_allowed_for_testing(true);

    UpdateSinks({cast_sink_1().sink()}, std::vector<url::Origin>());
    handler()->SetSinkIdForTesting(cast_sink_1().sink().id());

    EXPECT_CALL(*router(),
                CreateRouteInternal(source.id(), cast_sink_1().sink().id(), _,
                                    web_contents(), _, timeout));

    handler()->CastToSink(mock_callback.Get());
  }

  void StartPresentation(
      const content::PresentationRequest& request,
      std::unique_ptr<StartPresentationContext> start_presentation_context,
      MockCastToSinkCallback& mock_callback) {
    CreateHandler({MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR},
                  std::move(start_presentation_context));

    UpdateSinks({cast_sink_1().sink()}, {request.frame_origin});
    handler()->SetSinkIdForTesting(cast_sink_1().sink().id());

    auto source =
        MediaSource::ForPresentationUrl(*(request.presentation_urls.begin()));

    EXPECT_CALL(*router(),
                CreateRouteInternal(source.id(), cast_sink_1().sink().id(),
                                    request.frame_origin, web_contents(), _,
                                    base::Seconds(20)));
    handler()->CastToSink(mock_callback.Get());
  }

  std::unique_ptr<StartPresentationContext> CreateStartPresentationContext(
      content::PresentationRequest presentation_request,
      StartPresentationContext::PresentationConnectionCallback success_cb =
          base::DoNothing(),
      StartPresentationContext::PresentationConnectionErrorCallback error_cb =
          base::DoNothing()) {
    return std::make_unique<StartPresentationContext>(
        presentation_request, std::move(success_cb), std::move(error_cb));
  }

  void UpdateSinks(const std::vector<MediaSink>& sinks,
                   const std::vector<url::Origin>& origins) {
    for (MediaSinksObserver* sinks_observer : media_sinks_observers_) {
      sinks_observer->OnSinksUpdated(sinks, origins);
    }
  }

  void SignIn(signin::ConsentLevel consent_level) {
    CoreAccountInfo account_info =
        identity_test_env_.SetPrimaryAccount(kEmail, consent_level);
    sync_service_.SetSignedIn(consent_level, account_info);
  }

  void SetPausedSynServiceState() { sync_service_.SetPersistentAuthError(); }

  const MediaSinkInternal& cast_sink_1() { return cast_sink_1_; }
  const MediaSinkInternal& cast_sink_2() { return cast_sink_2_; }

 private:
  void InitializeMockMediaRouter() {
    logger_ = std::make_unique<LoggerImpl>();

    ON_CALL(*router(), GetLogger()).WillByDefault(Return(logger_.get()));
    // Store sink observers so that they can be notified in tests.
    ON_CALL(*router(), RegisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          media_sinks_observers_.push_back(observer);
          return true;
        });
    // Remove sink observers as appropriate (destructing handlers will cause
    // this to occur).
    ON_CALL(*router(), UnregisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          auto it = base::ranges::find(media_sinks_observers_, observer);
          if (it != media_sinks_observers_.end()) {
            media_sinks_observers_.erase(it);
          }
        });

    // Handler so MockMediaRouter will respond to requests to create a route.
    // Will construct a RouteRequestResult based on the set result code and
    // then call the handler's callback, which should call the page's callback.
    ON_CALL(*router(), CreateRouteInternal(_, _, _, _, _, _))
        .WillByDefault([this](const MediaSource::Id& source_id,
                              const MediaSink::Id& sink_id,
                              const url::Origin& origin,
                              content::WebContents* web_contents,
                              MediaRouteResponseCallback& callback,
                              base::TimeDelta timeout) {
          std::unique_ptr<RouteRequestResult> result;
          if (result_code_ == mojom::RouteRequestResultCode::OK) {
            MediaSource source(source_id);
            MediaRoute route;
            route.set_media_route_id(source_id + "->" + sink_id);
            route.set_media_source(source);
            route.set_media_sink_id(sink_id);
            result = RouteRequestResult::FromSuccess(route, std::string());
          } else {
            result = RouteRequestResult::FromError(std::string(), result_code_);
          }
          std::move(callback).Run(nullptr, *result);
        });
  }

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  raw_ptr<MockMediaRouter, AcrossTasksDanglingUntriaged> router_;
  std::unique_ptr<LoggerImpl> logger_;
  // `identity_test_env_` and `sync_service_` must stay private, so they are
  // always controlled together by SignIn().
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;

  static std::vector<DiscoveryNetworkInfo> GetFakeNetworkInfo() {
    return {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
    ;
  }

  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&GetFakeNetworkInfo);

  std::unique_ptr<AccessCodeCastHandler> handler_;
  std::unique_ptr<MockAccessCodeCastSinkService> access_code_cast_sink_service_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;

  TestMediaSinkService dual_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;

  NiceMock<cast_channel::MockCastMessageHandler> message_handler_;
  std::unique_ptr<StrictMock<MockPage>> page_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;
  std::unique_ptr<MockCastMediaSinkServiceImpl>
      mock_cast_media_sink_service_impl_;
  std::unique_ptr<MockWebContentsPresentationManager> presentation_manager_;
  std::vector<raw_ptr<MediaSinksObserver, VectorExperimental>>
      media_sinks_observers_;
  mojom::RouteRequestResultCode result_code_ =
      mojom::RouteRequestResultCode::OK;
  MediaSinkInternal cast_sink_1_;
  MediaSinkInternal cast_sink_2_;
};

TEST_F(AccessCodeCastHandlerTest, OnSinkAddedResult) {
  // OnSinkAddedResult should only trigger the callback if the channel opening
  // failed somehow.
  MockAddSinkCallback mock_callback_failure;
  handler()->SetSinkCallbackForTesting(mock_callback_failure.Get());

  EXPECT_CALL(mock_callback_failure,
              Run(AddSinkResultCode::CHANNEL_OPEN_ERROR));
  handler()->OnSinkAddedResultForTesting(AddSinkResultCode::CHANNEL_OPEN_ERROR,
                                         std::nullopt);
  EXPECT_FALSE(handler()->GetSinkIdForTesting().has_value());

  MockAddSinkCallback mock_callback_ok;
  handler()->SetSinkCallbackForTesting(mock_callback_ok.Get());

  EXPECT_CALL(mock_callback_ok, Run(AddSinkResultCode::OK)).Times(0);
  handler()->OnSinkAddedResultForTesting(AddSinkResultCode::OK, "123456");
  EXPECT_EQ(handler()->GetSinkIdForTesting().value(), "123456");
}

// Demonstrates that if the expected device is added to the media router,
// the page is notified of success.
TEST_F(AccessCodeCastHandlerTest, DiscoveredDeviceAdded) {
  MockAddSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());

  UpdateSinks({cast_sink_1().sink()}, std::vector<url::Origin>());
  MediaSinkWithCastModes sink_with_cast_modes(cast_sink_1().sink());
  sink_with_cast_modes.cast_modes = {MediaCastMode::DESKTOP_MIRROR};

  handler()->SetSinkIdForTesting(cast_sink_1().sink().id());
  handler()->OnSinksUpdatedForTesting({sink_with_cast_modes});
}

// Demonstrates that if handler is notified about a device other than the
// discovered device the page is not notified.
TEST_F(AccessCodeCastHandlerTest, OtherDevicesIgnored) {
  MockAddSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(_)).Times(Exactly(0));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());

  handler()->SetSinkIdForTesting(cast_sink_1().sink().id());

  MediaSinkWithCastModes sink_with_cast_modes(cast_sink_2().sink());
  sink_with_cast_modes.cast_modes = {MediaCastMode::DESKTOP_MIRROR};

  handler()->OnSinksUpdatedForTesting({sink_with_cast_modes});
}

// Demonstrates that desktop mirroring attempts call media router with the
// correct parameters, and that success is communicated to the dialog box.
TEST_F(AccessCodeCastHandlerTest, DesktopMirroring) {
  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(), mock_callback);
}

// Demonstrates that if casting does not start successfully that the error
// code is communicated to the dialog.
TEST_F(AccessCodeCastHandlerTest, DesktopMirroringError) {
  set_expected_cast_result(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::ROUTE_NOT_FOUND));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(), mock_callback);
}

// Demonstrates that tab mirroring attempts call media router with the
// correct parameters, and that success is communicated to the dialog box.
TEST_F(AccessCodeCastHandlerTest, TabMirroring) {
  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));
  MediaSource media_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  StartTabMirroring(media_source, mock_callback);
}

// Demonstrates that if casting does not start successfully that the error
// code is communicated to the dialog.
TEST_F(AccessCodeCastHandlerTest, TabMirroringError) {
  set_expected_cast_result(mojom::RouteRequestResultCode::INVALID_ORIGIN);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::INVALID_ORIGIN));
  MediaSource media_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  StartTabMirroring(media_source, mock_callback);
}

// Demonstrates that if a default presentation source is available,
// presentation casting will begin instead of tab casting.
TEST_F(AccessCodeCastHandlerTest, DefaultPresentation) {
  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));

  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://defaultpresentation.com")},
      url::Origin::Create(GURL("https://default.fakeurl")));
  presentation_manager()->SetDefaultPresentationRequest(presentation_request);
  StartPresentation(presentation_request, nullptr, mock_callback);
}

// Demonstrates that if a presentation casting does not start successfully
// that the error is propagated to the dialog.
TEST_F(AccessCodeCastHandlerTest, DefaultPresentationError) {
  set_expected_cast_result(mojom::RouteRequestResultCode::INVALID_ORIGIN);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::INVALID_ORIGIN));

  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://defaultpresentation.com")},
      url::Origin::Create(GURL("https://default.fakeurl")));
  presentation_manager()->SetDefaultPresentationRequest(presentation_request);
  StartPresentation(presentation_request, nullptr, mock_callback);
}

// Demonstrates that if a StartPresentationContext is supplied to the handler,
// it will be used to start casting in preference to the default request and
// tab mirroring.
TEST_F(AccessCodeCastHandlerTest, StartPresentationContext) {
  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));

  // Add a default presentation request. This should be ignored.
  content::PresentationRequest ignored_request(
      {0, 0}, {GURL("https://defaultpresentation.com")},
      url::Origin::Create(GURL("https://default.fakeurl")));
  presentation_manager()->SetDefaultPresentationRequest(ignored_request);

  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://startpresentrequest.com")},
      url::Origin::Create(GURL("https://start.fakeurl")));

  auto start_presentation_context =
      CreateStartPresentationContext(presentation_request);

  StartPresentation(presentation_request, std::move(start_presentation_context),
                    mock_callback);
}

// Demonstrates that casting will not start if there already exists a route for
// the given sink.
TEST_F(AccessCodeCastHandlerTest, RouteAlreadyExists) {
  MockCastToSinkCallback mock_callback;

  MediaSinkInternal access_code_sink = CreateCastSink(1);
  access_code_sink.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;

  CreateHandler({MediaCastMode::DESKTOP_MIRROR});
  set_screen_capture_allowed_for_testing(true);
  UpdateSinks({access_code_sink.sink()}, std::vector<url::Origin>());
  handler()->SetSinkIdForTesting(access_code_sink.sink().id());

  MediaRoute media_route_access = CreateRouteForTesting(access_code_sink.id());
  std::vector<MediaRoute> route_list = {media_route_access};
  ON_CALL(*router(), GetCurrentRoutes()).WillByDefault(Return(route_list));

  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::ROUTE_ALREADY_EXISTS));
  handler()->CastToSink(mock_callback.Get());
}

// Test that demonstrates profile sync error being called if sync is not enabled
// for the profile.
TEST_F(AccessCodeCastHandlerTest, ProfileSyncError) {
  MockAddSinkCallback mock_callback_failure;
  handler()->SetIdentityManagerForTesting(identity_manager());
  handler()->SetSyncServiceForTesting(&sync_service());

  SignIn(signin::ConsentLevel::kSignin);

  EXPECT_CALL(mock_callback_failure,
              Run(AddSinkResultCode::PROFILE_SYNC_ERROR));
  handler()->AddSink(
      "foo_code",
      access_code_cast::mojom::CastDiscoveryMethod::INPUT_ACCESS_CODE,
      mock_callback_failure.Get());
}

// Test that demonstrates profile sync error being called if sync is paused
// for the profile.
TEST_F(AccessCodeCastHandlerTest, ProfileSyncPaused) {
  MockAddSinkCallback mock_callback_failure;
  handler()->SetIdentityManagerForTesting(identity_manager());
  handler()->SetSyncServiceForTesting(&sync_service());
  SignIn(signin::ConsentLevel::kSync);
  SetPausedSynServiceState();

  EXPECT_CALL(mock_callback_failure,
              Run(AddSinkResultCode::PROFILE_SYNC_ERROR));
  handler()->AddSink(
      "foo_code",
      access_code_cast::mojom::CastDiscoveryMethod::INPUT_ACCESS_CODE,
      mock_callback_failure.Get());
}

// Test that demonstrates profile sync error is not called if sync is enabled
// for the profile.
TEST_F(AccessCodeCastHandlerTest, ProfileSyncSuccess) {
  MockAddSinkCallback mock_callback_success;
  handler()->SetIdentityManagerForTesting(identity_manager());
  handler()->SetSyncServiceForTesting(&sync_service());

  SignIn(signin::ConsentLevel::kSync);

  EXPECT_CALL(mock_callback_success, Run(AddSinkResultCode::UNKNOWN_ERROR))
      .Times(1);
  ON_CALL(*access_service(), DiscoverSink(_, _))
      .WillByDefault(
          [](const std::string& access_code,
             AccessCodeCastSinkService::AddSinkResultCallback callback) {
            std::move(callback).Run(AddSinkResultCode::UNKNOWN_ERROR,
                                    std::nullopt);
          });
  EXPECT_CALL(*access_service(), DiscoverSink(_, _)).Times(1);
  handler()->AddSink(
      "foo_code",
      access_code_cast::mojom::CastDiscoveryMethod::INPUT_ACCESS_CODE,
      mock_callback_success.Get());
}

// Demonstrates that adding a sink and successfully casting to it will trigger a
// histogram.
TEST_F(AccessCodeCastHandlerTest, SuccessfulAddAndCastMetric) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(histogram, 0);

  set_expected_cast_result(mojom::RouteRequestResultCode::OK);
  MockCastToSinkCallback mock_cast_sink_callback;
  EXPECT_CALL(mock_cast_sink_callback, Run(RouteRequestResultCode::OK));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(),
                        mock_cast_sink_callback);
  histogram_tester.ExpectTotalCount(histogram, 1);
}

}  // namespace media_router
