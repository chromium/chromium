// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/media_router/media_router_cast_ui_for_test.h"
#include "chrome/test/media_router/media_router_gmc_ui_for_test.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/issue.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::WebContents;
using testing::Optional;

namespace media_router {

namespace {

std::string GetStartedConnectionId(WebContents* web_contents) {
  return EvalJs(web_contents, "startedConnection.id").ExtractString();
}

std::string GetDefaultRequestSessionId(WebContents* web_contents) {
  return EvalJs(web_contents, "defaultRequestSessionId").ExtractString();
}

// Routes observer that calls a callback once there are no routes.
class NoRoutesObserver : public MediaRoutesObserver {
 public:
  NoRoutesObserver(MediaRouter* router, base::OnceClosure callback)
      : MediaRoutesObserver(router), callback_(std::move(callback)) {}

  ~NoRoutesObserver() override = default;

  void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override {
    if (callback_ && routes.empty())
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

MediaRouterIntegrationBrowserTest::MediaRouterIntegrationBrowserTest() {
  switch (GetParam()) {
    case UiForBrowserTest::kCast:
      feature_list_.InitAndDisableFeature(kGlobalMediaControlsCastStartStop);
      break;
    case UiForBrowserTest::kGmc:
      feature_list_.InitWithFeatures(
          {
              media::kGlobalMediaControls,
              kGlobalMediaControlsCastStartStop,
#if BUILDFLAG(IS_CHROMEOS_ASH)
              // Without this flag, SodaInstaller::GetInstance() fails a DCHECK
              // on Chrome OS. The call to SodaInstaller::GetInstance() is in
              // MediaDialogView::AddedToWidget(), which is called indirectly
              // from MediaDialogView::ShowDialogForPresentationRequest().
              ash::features::kOnDeviceSpeechRecognition,
#endif
#if !BUILDFLAG(IS_CHROMEOS)
              media::kGlobalMediaControlsUpdatedUI,
#endif
          },
          {});
      break;
  }
}

MediaRouterIntegrationBrowserTest::~MediaRouterIntegrationBrowserTest() =
    default;

Browser* MediaRouterIntegrationBrowserTest::browser() {
  return InProcessBrowserTest::browser();
}

void MediaRouterIntegrationBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      // Needed to allow a video to autoplay from a browser test.
      switches::autoplay::kNoUserGestureRequiredPolicy);
  if (!RequiresMediaRouteProviders()) {
    // Disable built-in media route providers.
    command_line->AppendSwitch(kDisableMediaRouteProvidersForTestSwitch);
  }
}

void MediaRouterIntegrationBrowserTest::SetUp() {
  ParseCommandLine();
  InProcessBrowserTest::SetUp();
}

void MediaRouterIntegrationBrowserTest::InitTestUi() {
  auto* const web_contents = GetActiveWebContents();
  auto* const context = browser()->profile();
  if (test_ui_) {
    test_ui_->TearDown();
  }
  switch (GetParam()) {
    case UiForBrowserTest::kCast:
      CHECK(!GlobalMediaControlsCastStartStopEnabled(context));
      test_ui_ = std::make_unique<MediaRouterCastUiForTest>(web_contents);
      break;
    case UiForBrowserTest::kGmc:
      CHECK(GlobalMediaControlsCastStartStopEnabled(context));
      test_ui_ = std::make_unique<MediaRouterGmcUiForTest>(web_contents);
      break;
  }
}

void MediaRouterIntegrationBrowserTest::TearDownOnMainThread() {
  test_ui_->TearDown();
  test_ui_.reset();
  test_provider_->TearDown();
  InProcessBrowserTest::TearDownOnMainThread();
  test_navigation_observer_.reset();
}

void MediaRouterIntegrationBrowserTest::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void MediaRouterIntegrationBrowserTest::SetUpOnMainThread() {
  MediaRouterDesktop* router = static_cast<MediaRouterDesktop*>(
      MediaRouterFactory::GetApiForBrowserContext(browser()->profile()));
  mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
  mojo::PendingRemote<mojom::MediaRouteProvider> provider_remote;
  router->BindToMojoReceiver(
      media_router_remote.InitWithNewPipeAndPassReceiver());
  test_provider_ = std::make_unique<TestMediaRouteProvider>(
      provider_remote.InitWithNewPipeAndPassReceiver(),
      std::move(media_router_remote));
  router->RegisterMediaRouteProvider(mojom::MediaRouteProviderId::TEST,
                                     std::move(provider_remote));

  InitTestUi();
}

bool MediaRouterIntegrationBrowserTest::ConditionalWait(
    base::TimeDelta timeout,
    base::TimeDelta interval,
    const base::RepeatingCallback<bool(void)>& callback) {
  base::ElapsedTimer timer;
  do {
    if (callback.Run())
      return true;

    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), interval);
    run_loop.Run();
  } while (timer.Elapsed() < timeout);

  return false;
}

void MediaRouterIntegrationBrowserTest::Wait(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), timeout);
  run_loop.Run();
}

void MediaRouterIntegrationBrowserTest::WaitUntilNoRoutes(
    WebContents* web_contents) {
  if (!test_provider_->HasRoutes())
    return;

  // TODO(crbug.com/1374499): There can't be a good reason to use the observer
  // API to check for routes asynchronously, which is fragile.  However, some
  // browser tests rely on this behavior.  Either add a callback parameter to
  // TerminateRoute, or add pass callback to the TestProvider to run when all
  // routes are gone.
  base::RunLoop run_loop;
  auto no_routes_observer = std::make_unique<NoRoutesObserver>(
      MediaRouterFactory::GetApiForBrowserContext(
          web_contents->GetBrowserContext()),
      run_loop.QuitClosure());
  run_loop.Run();
}

void MediaRouterIntegrationBrowserTest::ExecuteJavaScriptAPI(
    WebContents* web_contents,
    const std::string& script) {
  std::string result(EvalJs(web_contents, script).ExtractString());

  // Read the test result, the test result set by javascript is a
  // JSON string with the following format:
  // {"passed": "<true/false>", "errorMessage": "<error_message>"}
  std::optional<base::Value> value =
      base::JSONReader::Read(result, base::JSON_ALLOW_TRAILING_COMMAS);

  // Convert to dictionary.
  base::Value::Dict* dict_value = value->GetIfDict();
  ASSERT_TRUE(dict_value);

  // Extract the fields.
  const std::string* error_message = dict_value->FindString("errorMessage");
  ASSERT_TRUE(error_message);
  ASSERT_THAT(dict_value->FindBool("passed"), Optional(true))
      << error_message->c_str();
}

void MediaRouterIntegrationBrowserTest::StartSessionAndAssertNotFoundError() {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* web_contents = GetActiveWebContents();
  CHECK(web_contents);
  ExecuteJavaScriptAPI(web_contents, "startSession();");

  // Wait to simulate the user waiting for any sinks to be displayed.
  Wait(base::Seconds(1));
  test_ui_->HideDialog();
  CheckStartFailed(web_contents, "NotFoundError", "No screens found.");
}

WebContents*
MediaRouterIntegrationBrowserTest::StartSessionWithTestPageAndSink() {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* web_contents = GetActiveWebContents();
  CHECK(web_contents);
  ExecuteJavaScriptAPI(web_contents, "waitUntilDeviceAvailable();");
  ExecuteJavaScriptAPI(web_contents, "startSession();");
  test_ui_->WaitForDialogShown();
  return web_contents;
}

WebContents*
MediaRouterIntegrationBrowserTest::StartSessionWithTestPageAndChooseSink() {
  WebContents* web_contents = StartSessionWithTestPageAndSink();
  test_ui_->WaitForSinkAvailable(receiver_);
  test_ui_->StartCasting(receiver_);
  // TODO(takumif): Remove the HideDialog() call once the dialog can close
  // itself automatically after casting.
  test_ui_->HideDialog();
  return web_contents;
}

void MediaRouterIntegrationBrowserTest::OpenTestPage(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetTestPageUrl(full_path)));
}

void MediaRouterIntegrationBrowserTest::OpenTestPageInNewTab(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestPageUrl(full_path),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Opening a new tab creates new WebContents, so we must re-configure the
  // test UI for it.
  InitTestUi();
}

GURL MediaRouterIntegrationBrowserTest::GetTestPageUrl(
    const base::FilePath& full_path) {
  return net::FilePathToFileURL(full_path);
}

void MediaRouterIntegrationBrowserTest::CheckStartFailed(
    WebContents* web_contents,
    const std::string& error_name,
    const std::string& error_message_substring) {
  std::string script(base::StringPrintf("checkStartFailed('%s', '%s');",
                                        error_name.c_str(),
                                        error_message_substring.c_str()));
  ExecuteJavaScriptAPI(web_contents, script);
}

base::FilePath MediaRouterIntegrationBrowserTest::GetResourceFile(
    base::FilePath::StringPieceType relative_path) const {
  const base::FilePath full_path =
      base::PathService::CheckedGet(base::DIR_OUT_TEST_DATA_ROOT)
          .Append(FILE_PATH_LITERAL("media_router/browser_test_resources/"))
          .Append(relative_path);
  {
    // crbug.com/724573
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(PathExists(full_path));
  }
  return full_path;
}

void MediaRouterIntegrationBrowserTest::ExecuteScript(
    const content::ToRenderFrameHost& adapter,
    const std::string& script) {
  ASSERT_TRUE(content::ExecJs(adapter, script));
}

bool MediaRouterIntegrationBrowserTest::IsRouteCreatedOnUI() {
  return !test_ui_->GetRouteIdForSink(receiver_).empty();
}

void MediaRouterIntegrationBrowserTest::ParseCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  receiver_ = command_line->GetSwitchValueASCII("receiver");
  if (receiver_.empty())
    receiver_ = "test-sink-1";
}

void MediaRouterIntegrationBrowserTest::CheckSessionValidity(
    WebContents* web_contents) {
  ExecuteJavaScriptAPI(web_contents, "checkSession();");
  std::string session_id(GetStartedConnectionId(web_contents));
  EXPECT_FALSE(session_id.empty());
  std::string default_request_session_id(
      GetDefaultRequestSessionId(web_contents));
  EXPECT_EQ(session_id, default_request_session_id);
}

WebContents* MediaRouterIntegrationBrowserTest::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void MediaRouterIntegrationBrowserTest::RunBasicTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents,
                       "terminateSessionAndWaitForStateChange();");
  WaitUntilNoRoutes(web_contents);
}

void MediaRouterIntegrationBrowserTest::RunSendMessageTest(
    const std::string& message) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents,
                       base::StringPrintf("sendMessageAndExpectResponse('%s');",
                                          message.c_str()));
}

void MediaRouterIntegrationBrowserTest::RunFailToSendMessageTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, "closeConnectionAndWaitForStateChange();");
  ExecuteJavaScriptAPI(web_contents, "checkSendMessageFailed('closed');");
}

void MediaRouterIntegrationBrowserTest::RunReconnectSessionTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));
  OpenTestPageInNewTab(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* new_web_contents = GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf("reconnectSession('%s');", session_id.c_str()));
  ASSERT_EQ(session_id,
            content::EvalJs(new_web_contents, "reconnectedSession.id"));

  ExecuteJavaScriptAPI(web_contents,
                       "terminateSessionAndWaitForStateChange();");
  WaitUntilNoRoutes(web_contents);
}

void MediaRouterIntegrationBrowserTest::RunFailedReconnectSessionTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));

  OpenTestPageInNewTab(FILE_PATH_LITERAL("fail_reconnect_session.html"));
  WebContents* new_web_contents = GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);
  test_provider_->set_route_error_message("Unknown route");
  ExecuteJavaScriptAPI(new_web_contents,
                       base::StringPrintf("checkReconnectSessionFails('%s')",
                                          session_id.c_str()));
  ExecuteJavaScriptAPI(web_contents,
                       "terminateSessionAndWaitForStateChange();");
  WaitUntilNoRoutes(web_contents);
}

void MediaRouterIntegrationBrowserTest::SetEnableMediaRouter(bool enable) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kEnableMediaRouter, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(enable), nullptr);
  provider_.UpdateChromePolicy(policy);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterIntegrationBrowserTest::RunReconnectSessionSameTabTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));
  ExecuteJavaScriptAPI(web_contents, "closeConnectionAndWaitForStateChange();");

  ExecuteJavaScriptAPI(
      web_contents,
      base::StringPrintf("reconnectSession('%s');", session_id.c_str()));
  ASSERT_EQ(session_id, content::EvalJs(web_contents, "reconnectedSession.id"));
}

bool MediaRouterIntegrationBrowserTest::RequiresMediaRouteProviders() const {
  return false;
}

// TODO(crbug.com/1238758): Test is flaky on Windows and Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_Basic MANUAL_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest, MAYBE_Basic) {
  RunBasicTest();
}

// TODO(crbug.com/40784325): Test is flaky on Windows and Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_SendAndOnMessage MANUAL_SendAndOnMessage
#else
#define MAYBE_SendAndOnMessage SendAndOnMessage
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest,
                       MAYBE_SendAndOnMessage) {
  RunSendMessageTest("foo");
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest, CloseOnError) {
  test_provider_->set_close_route_error_on_send();
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents,
                       "sendMessageAndExpectConnectionCloseOnError()");
}

// TODO(crbug.com/40784296): Test is flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Fail_SendMessage MANUAL_Fail_SendMessage
#else
#define MAYBE_Fail_SendMessage Fail_SendMessage
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest,
                       MAYBE_Fail_SendMessage) {
  RunFailToSendMessageTest();
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest, Fail_CreateRoute) {
  test_provider_->set_route_error_message("Unknown sink");
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckStartFailed(web_contents, "UnknownError", "Unknown sink");
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest, ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest,
                       Fail_ReconnectSession) {
  RunFailedReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest, Fail_StartCancelled) {
  WebContents* web_contents = StartSessionWithTestPageAndSink();
  test_ui_->HideDialog();
  CheckStartFailed(web_contents, "NotAllowedError", "Dialog closed.");
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest,
                       Fail_StartCancelledNoSinks) {
  test_provider_->set_empty_sink_list();
  StartSessionAndAssertNotFoundError();
}

IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationBrowserTest,
                       Fail_StartCancelledNoSupportedSinks) {
  test_provider_->set_unsupported_media_sources_list();
  StartSessionAndAssertNotFoundError();
}

INSTANTIATE_MEDIA_ROUTER_INTEGRATION_BROWER_TEST_SUITE(
    MediaRouterIntegrationBrowserTest);

}  // namespace media_router
