// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_file_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/test_data_util.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace media_router {

namespace {
// Command line argument to specify receiver,
const char kReceiver[] = "receiver";
// The path relative to <chromium src>/out/<build config> for media router
// browser test resources.
const base::FilePath::StringPieceType kResourcePath = FILE_PATH_LITERAL(
    "media_router/browser_test_resources/");
const char kTestSinkName[] = "test-sink-1";
const char kButterflyVideoFileName[] = "butterfly-853x480.webm";
// The javascript snippets.
const char kCheckSessionScript[] = "checkSession();";
const char kCheckStartFailedScript[] = "checkStartFailed('%s', '%s');";
const char kStartSessionScript[] = "startSession();";
const char kTerminateSessionScript[] =
    "terminateSessionAndWaitForStateChange();";
const char kCloseSessionScript[] = "closeConnectionAndWaitForStateChange();";
const char kReconnectSessionScript[] = "reconnectSession('%s');";
const char kCheckSendMessageFailedScript[] = "checkSendMessageFailed('%s');";
const char kWaitSinkScript[] = "waitUntilDeviceAvailable();";
const char kSendMessageAndExpectResponseScript[] =
    "sendMessageAndExpectResponse('%s');";
const char kSendMessageAndExpectConnectionCloseOnErrorScript[] =
    "sendMessageAndExpectConnectionCloseOnError()";

std::string GetStartedConnectionId(WebContents* web_contents) {
  std::string session_id;
  CHECK(content::ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(startedConnection.id)",
      &session_id));
  return session_id;
}

std::string GetDefaultRequestSessionId(WebContents* web_contents) {
  std::string session_id;
  CHECK(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(defaultRequestSessionId)",
      &session_id));
  return session_id;
}

}  // namespace

MediaRouterIntegrationBrowserTest::MediaRouterIntegrationBrowserTest() {
}

MediaRouterIntegrationBrowserTest::~MediaRouterIntegrationBrowserTest() {
}

void MediaRouterIntegrationBrowserTest::TearDownOnMainThread() {
  test_ui_->TearDown();
  MediaRouterBaseBrowserTest::TearDownOnMainThread();
  test_navigation_observer_.reset();
}

void MediaRouterIntegrationBrowserTest::SetUpInProcessBrowserTestFixture() {
  MediaRouterBaseBrowserTest::SetUpInProcessBrowserTestFixture();
  EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void MediaRouterIntegrationBrowserTest::SetUpOnMainThread() {
  MediaRouterBaseBrowserTest::SetUpOnMainThread();
  // Dialogs created while MediaRouterUiForTest exists will not close on blur.
  test_ui_ =
      MediaRouterUiForTest::GetOrCreateForWebContents(GetActiveWebContents());
}

void MediaRouterIntegrationBrowserTest::ExecuteJavaScriptAPI(
    WebContents* web_contents,
    const std::string& script) {
  std::string result(ExecuteScriptAndExtractString(web_contents, script));

  // Read the test result, the test result set by javascript is a
  // JSON string with the following format:
  // {"passed": "<true/false>", "errorMessage": "<error_message>"}
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(
      result, base::JSON_ALLOW_TRAILING_COMMAS);

  // Convert to dictionary.
  base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  // Extract the fields.
  bool passed = false;
  ASSERT_TRUE(dict_value->GetBoolean("passed", &passed));
  std::string error_message;
  ASSERT_TRUE(dict_value->GetString("errorMessage", &error_message));

  ASSERT_TRUE(passed) << error_message;
}

void MediaRouterIntegrationBrowserTest::StartSessionAndAssertNotFoundError() {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* web_contents = GetActiveWebContents();
  CHECK(web_contents);
  ExecuteJavaScriptAPI(web_contents, kStartSessionScript);

  // Wait to simulate the user waiting for any sinks to be displayed.
  Wait(base::TimeDelta::FromSeconds(1));
  test_ui_->HideDialog();
  CheckStartFailed(web_contents, "NotFoundError", "No screens found.");
}

WebContents*
MediaRouterIntegrationBrowserTest::StartSessionWithTestPageAndSink() {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  WebContents* web_contents = GetActiveWebContents();
  CHECK(web_contents);
  ExecuteJavaScriptAPI(web_contents, kWaitSinkScript);
  ExecuteJavaScriptAPI(web_contents, kStartSessionScript);
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

void MediaRouterIntegrationBrowserTest::OpenDialogAndCastFile(
    bool route_success) {
  route_success
      ? SetTestData(FILE_PATH_LITERAL("local_media_sink.json"))
      : SetTestData(FILE_PATH_LITERAL("local_media_sink_route_fail.json"));
  GURL file_url = net::FilePathToFileURL(
      media::GetTestDataFilePath(kButterflyVideoFileName));
  test_ui_->ShowDialog();
  // Mock out file dialog operations, as those can't be simulated.
  test_ui_->SetLocalFile(file_url);
  test_ui_->WaitForSink(receiver_);
  test_ui_->ChooseSourceType(CastDialogView::kLocalFile);
  ASSERT_EQ(CastDialogView::kLocalFile, test_ui_->GetChosenSourceType());
  test_ui_->WaitForSinkAvailable(receiver_);
  test_ui_->StartCasting(receiver_);
  ASSERT_EQ(file_url, GetActiveWebContents()->GetURL());
}

void MediaRouterIntegrationBrowserTest::OpenDialogAndCastFileFails() {
  SetTestData(FILE_PATH_LITERAL("local_media_sink.json"));
  GURL file_url = net::FilePathToFileURL(
      media::GetTestDataFilePath(kButterflyVideoFileName));
  test_ui_->ShowDialog();
  // Mock out file dialog opperations, as those can't be simulated.
  test_ui_->SetLocalFileSelectionIssue(IssueInfo());
  test_ui_->WaitForSink(receiver_);
  test_ui_->ChooseSourceType(CastDialogView::kLocalFile);
  test_ui_->WaitForAnyIssue();
}

void MediaRouterIntegrationBrowserTest::OpenTestPage(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURL(browser(), GetTestPageUrl(full_path));
}

void MediaRouterIntegrationBrowserTest::OpenTestPageInNewTab(
    base::FilePath::StringPieceType file_name) {
  base::FilePath full_path = GetResourceFile(file_name);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestPageUrl(full_path),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  // Opening a new tab creates new WebContents, so we must re-configure the
  // test UI for it.
  test_ui_ =
      MediaRouterUiForTest::GetOrCreateForWebContents(GetActiveWebContents());
}

GURL MediaRouterIntegrationBrowserTest::GetTestPageUrl(
    const base::FilePath& full_path) {
  return net::FilePathToFileURL(full_path);
}

void MediaRouterIntegrationBrowserTest::CheckStartFailed(
    WebContents* web_contents,
    const std::string& error_name,
    const std::string& error_message_substring) {
  std::string script(base::StringPrintf(kCheckStartFailedScript,
                                        error_name.c_str(),
                                        error_message_substring.c_str()));
  ExecuteJavaScriptAPI(web_contents, script);
}

void MediaRouterIntegrationBrowserTest::SetTestData(
    base::FilePath::StringPieceType test_data_file) {
  base::FilePath full_path = GetResourceFile(test_data_file);
  JSONFileValueDeserializer deserializer(full_path);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value;
  {
    // crbug.com/724573
    base::ScopedAllowBlockingForTesting allow_blocking;
    value = deserializer.Deserialize(&error_code, &error_message);
  }
  CHECK(value.get()) << "Deserialize failed: " << error_message;
  std::string test_data_str;
  ASSERT_TRUE(base::JSONWriter::Write(*value, &test_data_str));
  ExecuteScriptInBackgroundPageNoWait(
      extension_id_,
      base::StringPrintf("localStorage['testdata'] = '%s'",
                         test_data_str.c_str()));
}

base::FilePath MediaRouterIntegrationBrowserTest::GetResourceFile(
    base::FilePath::StringPieceType relative_path) const {
  base::FilePath base_dir;
  // ASSERT_TRUE can only be used in void returning functions.
  // Use CHECK instead in non-void returning functions.
  CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
  base::FilePath full_path =
      base_dir.Append(kResourcePath).Append(relative_path);
  {
    // crbug.com/724573
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(PathExists(full_path));
  }
  return full_path;
}

int MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractInt(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  int result;
  CHECK(content::ExecuteScriptAndExtractInt(adapter, script, &result));
  return result;
}

std::string MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractString(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  std::string result;
  CHECK(content::ExecuteScriptAndExtractString(adapter, script, &result));
  return result;
}

bool MediaRouterIntegrationBrowserTest::ExecuteScriptAndExtractBool(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  bool result;
  CHECK(content::ExecuteScriptAndExtractBool(adapter, script, &result));
  return result;
}

void MediaRouterIntegrationBrowserTest::ExecuteScript(
    const content::ToRenderFrameHost& adapter, const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(adapter, script));
}

bool MediaRouterIntegrationBrowserTest::IsRouteCreatedOnUI() {
  return !test_ui_->GetRouteIdForSink(receiver_).empty();
}

bool MediaRouterIntegrationBrowserTest::IsUIShowingIssue() {
  std::string issue_text = test_ui_->GetIssueTextForSink(receiver_);
  return !issue_text.empty();
}

bool MediaRouterIntegrationBrowserTest::IsRouteClosedOnUI() {
  // After execute js script to close route on UI, the dialog will dispear
  // after 3s. But sometimes it takes more than 3s to close the route, so
  // we need to re-open the dialog if it is closed.
  if (!test_ui_->IsDialogShown())
    test_ui_->ShowDialog();
  test_ui_->WaitForSink(receiver_);
  return test_ui_->GetRouteIdForSink(receiver_).empty();
}

void MediaRouterIntegrationBrowserTest::ParseCommandLine() {
  MediaRouterBaseBrowserTest::ParseCommandLine();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  receiver_ = command_line->GetSwitchValueASCII(kReceiver);
  if (receiver_.empty())
    receiver_ = kTestSinkName;
}

void MediaRouterIntegrationBrowserTest::CheckSessionValidity(
    WebContents* web_contents) {
  ExecuteJavaScriptAPI(web_contents, kCheckSessionScript);
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
  ExecuteJavaScriptAPI(web_contents, kTerminateSessionScript);
  test_ui_->WaitUntilNoRoutes();
}

void MediaRouterIntegrationBrowserTest::RunSendMessageTest(
    const std::string& message) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(
      web_contents,
      base::StringPrintf(kSendMessageAndExpectResponseScript, message.c_str()));
}

void MediaRouterIntegrationBrowserTest::RunFailToSendMessageTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kCloseSessionScript);

  ExecuteJavaScriptAPI(
      web_contents,
      base::StringPrintf(kCheckSendMessageFailedScript, "closed"));
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
      base::StringPrintf(kReconnectSessionScript, session_id.c_str()));
  std::string reconnected_session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      new_web_contents,
      "window.domAutomationController.send(reconnectedSession.id)",
      &reconnected_session_id));
  ASSERT_EQ(session_id, reconnected_session_id);

  ExecuteJavaScriptAPI(web_contents, kTerminateSessionScript);
  test_ui_->WaitUntilNoRoutes();
}

void MediaRouterIntegrationBrowserTest::SetEnableMediaRouter(bool enable) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kEnableMediaRouter, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(enable), nullptr);
  provider_.UpdateChromePolicy(policy);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterIntegrationBrowserTest::RunReconnectSessionSameTabTest() {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));
  ExecuteJavaScriptAPI(web_contents, kCloseSessionScript);

  ExecuteJavaScriptAPI(web_contents, base::StringPrintf(kReconnectSessionScript,
                                                        session_id.c_str()));
  std::string reconnected_session_id;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(reconnectedSession.id)",
      &reconnected_session_id));
  ASSERT_EQ(session_id, reconnected_session_id);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, Basic) {
  RunBasicTest();
}

// Tests that creating a route with a local file opens the file in a new tab.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       OpenLocalMediaFileInCurrentTab) {
  // Start at a new tab, the file should open in the same tab.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  // Make sure there is 1 tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  OpenDialogAndCastFile();

  // Expect that no new tab has been opened.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // The dialog will close from navigating to the local file within the tab, so
  // open it again after it closes.
  test_ui_->WaitForDialogHidden();
  test_ui_->ShowDialog();

  // Wait for a route to be created.
  test_ui_->WaitForAnyRoute();
}

// Tests that creating a route with a local file opens the file in a new tab.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       OpenLocalMediaFileInNewTab) {
  // Start at a tab with content in it, the file will open in a new tab.
  ui_test_utils::NavigateToURL(browser(), GURL("https://google.com"));
  // Make sure there is 1 tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  OpenDialogAndCastFile();

  // Expect that a new tab has been opened.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  test_ui_->ShowDialog();

  // Wait for a route to be created.
  test_ui_->WaitForAnyRoute();
}

// Tests that failing to create a route with a local file shows an issue.
// TODO(https://crbug.com/907539): Make the Views dialog show the issue.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       DISABLED_OpenLocalMediaFileFailsAndShowsIssue) {
  OpenDialogAndCastFileFails();
  // Expect that the issue is showing.
  ASSERT_TRUE(IsUIShowingIssue());
}

// Tests that creating a route with a local file opens in fullscreen.
// TODO(https://crbug.com/903016) Disabled due to flakiness in entering
// fullscreen.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       DISABLED_OpenLocalMediaFileFullscreen) {
  // Start at a new tab, the file should open in the same tab.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  // Make sure there is 1 tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  OpenDialogAndCastFile();

  // Increment web contents capturer count so it thinks capture has started.
  // This will allow the file tab to go fullscreen.
  content::WebContents* web_contents = GetActiveWebContents();
  web_contents->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ false);

  // Wait for capture poll timer to pick up change.
  Wait(base::TimeDelta::FromSeconds(3));

  // Expect that fullscreen was entered.
  ASSERT_TRUE(
      web_contents->GetDelegate()->IsFullscreenForTabOrPending(web_contents));
}

// Tests that failed route creation of local file does not enter fullscreen.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       OpenLocalMediaFileCastFailNoFullscreen) {
  // Start at a new tab, the file should open in the same tab.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  // Make sure there is 1 tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  OpenDialogAndCastFile(false);

  // Wait for file to start playing (but not being captured).
  Wait(base::TimeDelta::FromSeconds(3));

  // Expect no capture is ongoing.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_FALSE(web_contents->IsBeingCaptured());

  // Expect that fullscreen is not entered.
  ASSERT_FALSE(
      web_contents->GetDelegate()->IsFullscreenForTabOrPending(web_contents));
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, SendAndOnMessage) {
  RunSendMessageTest("foo");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, CloseOnError) {
  SetTestData(FILE_PATH_LITERAL("close_route_with_error_on_send.json"));
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents,
                       kSendMessageAndExpectConnectionCloseOnErrorScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, Fail_SendMessage) {
  RunFailToSendMessageTest();
}

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Fail_NoProvider) {
  SetTestData(FILE_PATH_LITERAL("no_provider.json"));
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckStartFailed(web_contents, "UnknownError",
                   "No provider supports createRoute with source");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, Fail_CreateRoute) {
  SetTestData(FILE_PATH_LITERAL("fail_create_route.json"));
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckStartFailed(web_contents, "UnknownError", "Unknown sink");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       Fail_ReconnectSession) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  std::string session_id(GetStartedConnectionId(web_contents));

  SetTestData(FILE_PATH_LITERAL("fail_reconnect_session.json"));
  OpenTestPageInNewTab(FILE_PATH_LITERAL("fail_reconnect_session.html"));
  WebContents* new_web_contents = GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ExecuteJavaScriptAPI(
      new_web_contents,
      base::StringPrintf("checkReconnectSessionFails('%s');",
                         session_id.c_str()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, Fail_StartCancelled) {
  WebContents* web_contents = StartSessionWithTestPageAndSink();
  test_ui_->HideDialog();
  CheckStartFailed(web_contents, "NotAllowedError", "Dialog closed.");
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       Fail_StartCancelledNoSinks) {
  SetTestData(FILE_PATH_LITERAL("no_sinks.json"));
  StartSessionAndAssertNotFoundError();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       Fail_StartCancelledNoSupportedSinks) {
  SetTestData(FILE_PATH_LITERAL("no_supported_sinks.json"));
  StartSessionAndAssertNotFoundError();
}

void MediaRouterIntegrationIncognitoBrowserTest::InstallAndEnableMRExtension() {
  const extensions::Extension* extension =
      LoadExtensionIncognito(extension_unpacked_);
  incognito_extension_id_ = extension->id();
}

void MediaRouterIntegrationIncognitoBrowserTest::UninstallMRExtension() {
  if (!incognito_extension_id_.empty()) {
    UninstallExtension(incognito_extension_id_);
  }
}

Browser* MediaRouterIntegrationIncognitoBrowserTest::browser() {
  if (!incognito_browser_)
    incognito_browser_ = CreateIncognitoBrowser();
  return incognito_browser_;
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationIncognitoBrowserTest, Basic) {
  RunBasicTest();
  // If we tear down before route observers are notified of route termination,
  // MediaRouter will create another TerminateRoute() request which will have a
  // dangling Mojo callback at shutdown. So we must wait for the update.
  test_ui_->WaitUntilNoRoutes();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationIncognitoBrowserTest,
                       ReconnectSession) {
  RunReconnectSessionTest();
  // If we tear down before route observers are notified of route termination,
  // MediaRouter will create another TerminateRoute() request which will have a
  // dangling Mojo callback at shutdown. So we must wait for the update.
  test_ui_->WaitUntilNoRoutes();
}

}  // namespace media_router
