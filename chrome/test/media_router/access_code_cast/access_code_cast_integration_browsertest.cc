// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/access_code_cast/access_code_cast_integration_browsertest.h"

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_tags.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/barrier_closure.h"
#include "base/json/values_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_lacros.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_impl.h"
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/cast_config/cast_config_controller_media_router.h"
#endif

using testing::_;
namespace {
class TestMediaRouter : public media_router::MockMediaRouter {
 public:
  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<TestMediaRouter>();
  }

  media_router::LoggerImpl* GetLogger() override {
    if (!logger_)
      logger_ = std::make_unique<media_router::LoggerImpl>();
    return logger_.get();
  }

 private:
  std::unique_ptr<media_router::LoggerImpl> logger_;
};

class CloseObserver : public content::WebContentsObserver {
 public:
  explicit CloseObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  CloseObserver(const CloseObserver&) = delete;
  CloseObserver& operator=(const CloseObserver&) = delete;

  void Wait() { close_loop_.Run(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { close_loop_.Quit(); }

 private:
  base::RunLoop close_loop_;
};

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

namespace media_router {

AccessCodeCastIntegrationBrowserTest::AccessCodeCastIntegrationBrowserTest()
    : url_to_intercept_(std::string(kDefaultDiscoveryEndpoint) +
                        kDiscoveryServicePath),
      mock_cast_socket_service_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  // TODO(crbug.com/323780452): Remove performance manager feature after deflake
  feature_list_.InitWithFeatures(
      {features::kAccessCodeCastUI,
       performance_manager::features::
           kBackgroundTabLoadingFromPerformanceManager},
      {});
  task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
}

AccessCodeCastIntegrationBrowserTest::~AccessCodeCastIntegrationBrowserTest() =
    default;

void AccessCodeCastIntegrationBrowserTest::SetUp() {
// This makes sure CastDeviceCache is not initialized until after the
// MockMediaRouter is ready. (MockMediaRouter can't be constructed yet.)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CastConfigControllerMediaRouter::SetMediaRouterForTest(nullptr);
#endif
  InProcessBrowserTest::SetUp();

  // This command removes the verify pixels switch so that our TestDialog code
  // does not automatically take pixel screenshots.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kVerifyPixels);
}

void AccessCodeCastIntegrationBrowserTest::SetUpInProcessBrowserTestFixture() {
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&AccessCodeCastIntegrationBrowserTest::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void AccessCodeCastIntegrationBrowserTest::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&CreateTestSyncService));

  media_router_ = static_cast<TestMediaRouter*>(
      media_router::MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating(&TestMediaRouter::Create)));

  ON_CALL(*media_router_, RegisterMediaSinksObserver(_))
      .WillByDefault([this](MediaSinksObserver* observer) {
        media_sinks_observers_.push_back(observer);
        return true;
      });

  // Remove sink observers as appropriate (destructing handlers will cause
  // this to occur).
  ON_CALL(*media_router_, UnregisterMediaSinksObserver(_))
      .WillByDefault([this](MediaSinksObserver* observer) {
        auto it = base::ranges::find(media_sinks_observers_, observer);
        if (it != media_sinks_observers_.end()) {
          media_sinks_observers_.erase(it);
        }
      });

  // Handler so MockMediaRouter will respond to requests to create a route.
  // Will construct a RouteRequestResult based on the set result code and
  // then call the handler's callback, which should call the page's callback.
  ON_CALL(*media_router_, CreateRouteInternal(_, _, _, _, _, _))
      .WillByDefault(
          [this](const MediaSource::Id& source_id, const MediaSink::Id& sink_id,
                 const url::Origin& origin, content::WebContents* web_contents,
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
              result =
                  RouteRequestResult::FromError(std::string(), result_code_);
            }
            std::move(callback).Run(nullptr, *result);
          });

  // We must create the CastMediaSinkServiceImpl before the
  // AccessCodeCastSinkService is generated.
  if (!impl_)
    impl_ = CreateImpl();

  AccessCodeCastSinkServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&AccessCodeCastIntegrationBrowserTest::
                                       CreateAccessCodeCastSinkService,
                                   base::Unretained(this)));
}

void AccessCodeCastIntegrationBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  network_connection_tracker_ =
      network::TestNetworkConnectionTracker::CreateInstance();
  content::SetNetworkConnectionTrackerForTesting(nullptr);
  content::SetNetworkConnectionTrackerForTesting(
      network_connection_tracker_.get());
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  url_loader_interceptor_ =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          &AccessCodeCastIntegrationBrowserTest::InterceptRequest,
          base::Unretained(this)));
  identity_test_environment_ =
      std::make_unique<signin::IdentityTestEnvironment>();
  // In case of multiple BrowserContext created, we should reassign
  // `media_router_` to the one associated with `browser()->profile()`.
  if (browser()) {
    media_router_ = static_cast<TestMediaRouter*>(
        media_router::MediaRouterFactory::GetInstance()
            ->MediaRouterFactory::GetApiForBrowserContext(
                browser()->profile()));
  }

  // Support multiple sites on the test server.
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);

  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->StartAcceptingConnections();
}

void AccessCodeCastIntegrationBrowserTest::SetUpPrimaryAccountWithHostedDomain(
    signin::ConsentLevel consent_level,
    Profile* profile,
    bool sign_in_account) {
  ASSERT_TRUE(identity_test_environment_);
  // Ensure that the stub user is signed in.
  identity_test_environment_->MakePrimaryAccountAvailable(
      user_manager::kStubUserEmail, consent_level);

  if (sign_in_account) {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile),
        user_manager::kStubUserEmail, consent_level);
  }

  identity_test_environment_->SetAutomaticIssueOfAccessTokens(true);

  ASSERT_TRUE(AccessCodeCastSinkServiceFactory::GetForProfile(profile));
  AccessCodeCastSinkServiceFactory::GetForProfile(profile)
      ->SetIdentityManagerForTesting(
          identity_test_environment_->identity_manager());

  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      sync_service(profile)->SetPersistentAuthError();
      break;
    case signin::ConsentLevel::kSync:
      sync_service(profile)->SetMaxTransportState(
          syncer::SyncService::TransportState::ACTIVE);
      break;
  }

  base::RunLoop().RunUntilIdle();
}

void AccessCodeCastIntegrationBrowserTest::EnableAccessCodeCasting() {
  browser()->profile()->GetPrefs()->SetBoolean(
      media_router::prefs::kAccessCodeCastEnabled, true);
  base::RunLoop().RunUntilIdle();
}

void AccessCodeCastIntegrationBrowserTest::ShowUi(const std::string& name) {
  CastModeSet tab_mode = {MediaCastMode::TAB_MIRROR};
  std::unique_ptr<MediaRouteStarter> starter =
      std::make_unique<MediaRouteStarter>(
          MediaRouterUIParameters(tab_mode, web_contents()));
  AccessCodeCastDialog::Show(
      tab_mode, std::move(starter),
      AccessCodeCastDialogOpenLocation::kBrowserCastMenu);
}

content::WebContents* AccessCodeCastIntegrationBrowserTest::ShowDialog() {
  content::WebContentsAddedObserver observer;
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Setting the flag to prevent widget deactivation before creating the dialog.
  AccessCodeCastDialog::ShouldBlockWidgetActivationChangedForTest(true);

  // This string is empty since the ShowUi function requires a string. We do not
  // need one in the context we are using the function.
  ShowUi("");
  EXPECT_TRUE(VerifyUi());
  content::WebContents* dialog_contents = observer.GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));

  return dialog_contents;
}

void AccessCodeCastIntegrationBrowserTest::CloseDialog(
    content::WebContents* dialog_contents) {
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(ExecJs(dialog_contents, std::string(GetElementScript()) +
                                          ".cancelButtonPressed();"));
}

void AccessCodeCastIntegrationBrowserTest::SetAccessCode(
    std::string access_code,
    content::WebContents* dialog_contents) {
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(
      ExecJs(dialog_contents, GetElementScript() + ".switchToCodeInput();"));
  EXPECT_TRUE(ExecJs(
      dialog_contents,
      GetElementScript() + ".setAccessCodeForTest('" + access_code + "');"));
}

void AccessCodeCastIntegrationBrowserTest::PressSubmit(
    content::WebContents* dialog_contents) {
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(
      ExecJs(dialog_contents, GetElementScript() + ".addSinkAndCast();"));
}

void AccessCodeCastIntegrationBrowserTest::PressSubmitAndWaitForClose(
    content::WebContents* dialog_contents) {
  CloseObserver close_observer(dialog_contents);
  PressSubmit(dialog_contents);
  close_observer.Wait();
}

void AccessCodeCastIntegrationBrowserTest::CloseDialogUsingKeyPress() {
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::KeyboardCode::VKEY_ESCAPE, false, false, false, false));
}

void AccessCodeCastIntegrationBrowserTest::SetAccessCodeUsingKeyPress(
    const std::string& access_code) {
  for (char letter : access_code) {
    const auto keyboard_code = static_cast<ui::KeyboardCode>(
        absl::ascii_toupper(static_cast<unsigned char>(letter)));
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), keyboard_code, false,
                                                false, false, false));
  }
}

void AccessCodeCastIntegrationBrowserTest::PressSubmitUsingKeyPress() {
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::KeyboardCode::VKEY_RETURN, false, false, false, false));
}

void AccessCodeCastIntegrationBrowserTest::
    PressSubmitAndWaitForCloseUsingKeyPress(
        content::WebContents* dialog_contents) {
  CloseObserver close_observer(dialog_contents);
  PressSubmitUsingKeyPress();
  close_observer.Wait();
}

int AccessCodeCastIntegrationBrowserTest::WaitForAddSinkErrorCode(
    content::WebContents* dialog_contents) {
  // Spin the run loop until we get any error code (0 represents no error).
  while (0 ==
         EvalJs(GetErrorElementScript() + ".getMessageCode();", dialog_contents)
             .ExtractInt()) {
    SpinRunLoop(base::Milliseconds(20));
  }
  return EvalJs(GetErrorElementScript() + ".getMessageCode();", dialog_contents)
      .ExtractInt();
}

bool AccessCodeCastIntegrationBrowserTest::HasSinkInDevicesDict(
    const MediaSink::Id& sink_id) {
  base::test::TestFuture<base::Value::Dict> media_sink;
  GetPrefUpdater()->GetMediaSinkInternalValueBySinkId(sink_id,
                                                      media_sink.GetCallback());

  return !media_sink.Get().empty();
}

std::optional<base::Time>
AccessCodeCastIntegrationBrowserTest::GetDeviceAddedTimeFromDict(
    const MediaSink::Id& sink_id) {
  if (!GetPrefUpdater()) {
    return std::nullopt;
  }

  base::test::TestFuture<std::optional<base::Time>> time;
  GetPrefUpdater()->GetDeviceAddedTime(sink_id, time.GetCallback());
  return time.Get();
}

void AccessCodeCastIntegrationBrowserTest::TearDownOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Clear the prefs value manually on Lacros because Lacros is querying for
  // prefs stored in Ash, which doesn't reset after each test finishes.
  // The prefs value should not be cleared for tests that span a browser restart
  // because it's intended to keep the user data directory.
  if (GetTestPreCount() == 0 && IsAccessCodeCastLacrosSyncEnabled()) {
    auto* pref_updater = GetPrefUpdater();
    if (pref_updater) {
      base::RunLoop run_loop;
      auto barrier_callback = base::BarrierClosure(2, run_loop.QuitClosure());
      pref_updater->ClearDevicesDict(barrier_callback);
      pref_updater->ClearDeviceAddedTimeDict(barrier_callback);
      run_loop.Run();
    }
  }
#endif

  url_loader_interceptor_.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  InProcessBrowserTest::TearDownOnMainThread();
}

void AccessCodeCastIntegrationBrowserTest::ExpectMediaRouterHasNoSinks(
    base::OnceClosure callback,
    bool has_sink) {
  EXPECT_FALSE(has_sink);
  std::move(callback).Run();
}

void AccessCodeCastIntegrationBrowserTest::ExpectMediaRouterHasSink(
    base::OnceClosure callback,
    bool has_sink) {
  EXPECT_TRUE(has_sink);
  std::move(callback).Run();
}

std::unique_ptr<KeyedService>
AccessCodeCastIntegrationBrowserTest::CreateAccessCodeCastSinkService(
    content::BrowserContext* context) {
  auto* media_router = static_cast<TestMediaRouter*>(
      media_router::MediaRouterFactory::GetInstance()
          ->MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(media_router);
  DCHECK(mock_cast_media_sink_service_impl());

  Profile* profile = Profile::FromBrowserContext(context);
  return base::WrapUnique(new AccessCodeCastSinkService(
      profile, media_router, mock_cast_media_sink_service_impl(),
      DiscoveryNetworkMonitor::GetInstance(), profile->GetPrefs(), nullptr));
}

MockCastMediaSinkServiceImpl*
AccessCodeCastIntegrationBrowserTest::CreateImpl() {
  mock_cast_socket_service_ =
      std::unique_ptr<cast_channel::MockCastSocketService,
                      base::OnTaskRunnerDeleter>(
          new cast_channel::MockCastSocketService(
              (content::GetIOThreadTaskRunner({}))),
          base::OnTaskRunnerDeleter(content::GetIOThreadTaskRunner({})));

  MockCastMediaSinkServiceImpl* cast_media_sink_service_impl =
      new MockCastMediaSinkServiceImpl(OnSinksDiscoveredCallback(),
                                       mock_cast_socket_service_.get(),
                                       DiscoveryNetworkMonitor::GetInstance(),
                                       mock_dual_media_sink_service_.get());

  ON_CALL(*cast_media_sink_service_impl, OpenChannel(_, _, _, _, _))
      .WillByDefault(testing::Invoke(
          this,
          &AccessCodeCastIntegrationBrowserTest::MockOnChannelOpenedCall));

  ON_CALL(*cast_media_sink_service_impl, HasSink(_))
      .WillByDefault(testing::Invoke([this](const MediaSink::Id& sink_id) {
        return base::Contains(added_sink_ids_, sink_id);
      }));

  // TODO(b/242777549): Properly delete the cast_media_sink_service_impl instead
  // of allowing leak.
  testing::Mock::AllowLeak(cast_media_sink_service_impl);
  return cast_media_sink_service_impl;
}

void AccessCodeCastIntegrationBrowserTest::MockOnChannelOpenedCall(
    const MediaSinkInternal& cast_sink,
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    CastDeviceCountMetrics::SinkSource sink_source,
    ChannelOpenedCallback callback,
    cast_channel::CastSocketOpenParams open_params) {
  if (!open_channel_response_) {
    std::move(callback).Run(open_channel_response_);
    return;
  }

  // On a successful addition to the media router, we have to mock
  // the channel open response within the Media Router AND that the
  // sink has been added to the QueryResultsManager. We achieve this
  // by notifying observers that a sink was successfully added.
  std::vector<media_router::MediaSink> one_sink;
  one_sink.push_back(cast_sink.sink());
  added_sink_ids_.insert(cast_sink.id());

  mock_cast_media_sink_service_impl()->task_runner().get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockCastMediaSinkServiceImpl::AddSinkForTest,
                     base::Unretained(mock_cast_media_sink_service_impl()),
                     cast_sink));

  if (cast_sink.cast_data().discovery_type ==
      CastDiscoveryType::kAccessCodeRememberedDevice) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AccessCodeCastIntegrationBrowserTest::UpdateDeviceAddedTime,
            base::Unretained(this), cast_sink.id()));
  }

  // The open channel callback needs to run after the AddSinkForTest is posted
  // to ensure that no race conditions occur and we mimic an actual access code
  // casting situation.
  std::move(callback).Run(open_channel_response_);

  // A delay is added to the QRM notification since this
  // simulates the non-instant time it takes for a sink to be added
  // to the QRM.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccessCodeCastIntegrationBrowserTest::UpdateSinks,
                     base::Unretained(this), one_sink,
                     std::vector<url::Origin>()),
      base::Milliseconds(20));
}

void AccessCodeCastIntegrationBrowserTest::SpinRunLoop(base::TimeDelta delay) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delay);
  run_loop.Run();
}

content::EvalJsResult AccessCodeCastIntegrationBrowserTest::EvalJs(
    const std::string& string_value,
    content::WebContents* web_contents) {
  return content::EvalJs(web_contents, string_value,
                         content::EXECUTE_SCRIPT_DEFAULT_OPTIONS);
}

void AccessCodeCastIntegrationBrowserTest::SetEndpointFetcherMockResponse(
    const std::string& response_data,
    net::HttpStatusCode response_code,
    net::Error error) {
  should_intercept_response_ = true;
  response_data_ = response_data;
  response_code_ = response_code;
  error_ = error;
}

// URLLoaderInterceptor callback
bool AccessCodeCastIntegrationBrowserTest::InterceptRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  DCHECK(params);
  if (!should_intercept_response_)
    return false;

  // Check to see if it is a cast2class url.
  if (!base::StartsWith(params->url_request.url.spec(), url_to_intercept_,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  std::string headers(base::StringPrintf(
      "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
      static_cast<int>(response_code_), GetHttpReasonPhrase(response_code_)));

  network::URLLoaderCompletionStatus status(error_);
  status.decoded_body_length = response_data_.size();

  content::URLLoaderInterceptor::WriteResponse(headers, response_data_,
                                               params->client.get());
  params->client->OnComplete(status);

  return true;
}

void AccessCodeCastIntegrationBrowserTest::SetMockOpenChannelCallbackResponse(
    bool channel_opened) {
  open_channel_response_ = channel_opened;
}

void AccessCodeCastIntegrationBrowserTest::UpdateSinks(
    const std::vector<MediaSink>& sinks,
    const std::vector<url::Origin>& origins) {
  for (MediaSinksObserver* sinks_observer : media_sinks_observers_) {
    sinks_observer->OnSinksUpdated(sinks, origins);
  }
}

void AccessCodeCastIntegrationBrowserTest::UpdateRoutes(
    const std::vector<MediaRoute>& routes) {
  for (MediaRoutesObserver& routes_observer :
       media_router_->routes_observers()) {
    routes_observer.OnRoutesUpdated(routes);
  }
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
}

void AccessCodeCastIntegrationBrowserTest::ExpectStartRouteCallFromTabMirroring(
    const std::string& sink_name,
    const std::string& media_source_id,
    content::WebContents* web_contents,
    base::TimeDelta timeout,
    media_router::MockMediaRouter* media_router) {
  if (!media_router) {
    media_router = static_cast<TestMediaRouter*>(
        media_router::MediaRouterFactory::GetInstance()
            ->GetApiForBrowserContext(browser()->profile()));
  }
  EXPECT_CALL(*media_router, CreateRouteInternal(media_source_id, sink_name, _,
                                                 web_contents, _, timeout));
}

AccessCodeCastPrefUpdater*
AccessCodeCastIntegrationBrowserTest::GetPrefUpdater() {
  auto* service = AccessCodeCastSinkServiceFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
  return service ? service->GetPrefUpdaterForTesting() : nullptr;
}

void AccessCodeCastIntegrationBrowserTest::AddScreenplayTag(
    const std::string& screenplay_tag) {
  base::AddTagToTestResult("feature_id", screenplay_tag);
}

void AccessCodeCastIntegrationBrowserTest::UpdateDeviceAddedTime(
    const MediaSink::Id& sink_id) {
  // Record the device added time of saved sinks to verify that this does not
  // change when the channel is opened.
  // We should not use Run() + QuitClosure() here because that will block the
  // test.
  if (!GetPrefUpdater()) {
    return;
  }

  GetPrefUpdater()->GetDeviceAddedTime(
      sink_id,
      base::BindLambdaForTesting([this](std::optional<base::Time> time) {
        if (time.has_value()) {
          this->device_added_time_ = time.value();
        }
      }));
}

void AccessCodeCastIntegrationBrowserTest::
    SetAccessCodeCastSinkServiceTaskRunner() {
  auto* service = AccessCodeCastSinkServiceFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
  if (service) {
    service->SetTaskRunnerForTesting(task_runner_);
  }
}

bool AccessCodeCastIntegrationBrowserTest::IsAccessCodeCastLacrosSyncEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::test::TestFuture<std::optional<base::Value>> future;
  chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      crosapi::mojom::PrefPath::kAccessCodeCastDevices, future.GetCallback());
  return future.Take().has_value();
#else
  return false;
#endif
}

}  // namespace media_router
