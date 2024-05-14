// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ppapi/ppapi_test.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gl/gl_switches.h"

using content::RenderViewHost;
using content::TestMessageHandler;

namespace {

void AddPrivateSwitches(base::CommandLine* command_line) {
  // For TestRequestOSFileHandle.
  command_line->AppendSwitch(switches::kUnlimitedStorage);
  command_line->AppendSwitchASCII(switches::kAllowNaClFileHandleAPI,
                                  "127.0.0.1");
}

}  // namespace

PPAPITestMessageHandler::PPAPITestMessageHandler() {
}

TestMessageHandler::MessageResponse PPAPITestMessageHandler::HandleMessage(
    const std::string& json) {
  std::string trimmed;
  base::TrimString(json, "\"", &trimmed);
  if (trimmed == "...")
    return CONTINUE;
  message_ = trimmed;
  return DONE;
}

void PPAPITestMessageHandler::Reset() {
  TestMessageHandler::Reset();
  message_.clear();
}

PPAPITestBase::InfoBarObserver::InfoBarObserver(PPAPITestBase* test_base)
    : test_base_(test_base), expecting_infobar_(false), should_accept_(false) {
  infobar_observation_.Observe(GetInfoBarManager());
}

PPAPITestBase::InfoBarObserver::~InfoBarObserver() {
  EXPECT_FALSE(expecting_infobar_) << "Missing an expected infobar";
}

void PPAPITestBase::InfoBarObserver::ExpectInfoBarAndAccept(
    bool should_accept) {
  ASSERT_FALSE(expecting_infobar_);
  expecting_infobar_ = true;
  should_accept_ = should_accept;
}

void PPAPITestBase::InfoBarObserver::OnInfoBarAdded(
    infobars::InfoBar* infobar) {
  // It's not safe to remove the infobar here, since other observers (e.g. the
  // InfoBarContainer) may still need to access it.  Instead, post a task to
  // do all necessary infobar manipulation as soon as this call stack returns.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InfoBarObserver::VerifyInfoBarState,
                                base::Unretained(this)));
}

void PPAPITestBase::InfoBarObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  ASSERT_TRUE(infobar_observation_.IsObservingSource(manager));
  infobar_observation_.Reset();
}

void PPAPITestBase::InfoBarObserver::VerifyInfoBarState() {
  infobars::ContentInfoBarManager* infobar_manager = GetInfoBarManager();
  EXPECT_EQ(expecting_infobar_ ? 1U : 0U, infobar_manager->infobars().size());
  if (!expecting_infobar_)
    return;
  expecting_infobar_ = false;

  infobars::InfoBar* infobar = infobar_manager->infobars()[0];
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate != nullptr);
  if (should_accept_)
    delegate->Accept();
  else
    delegate->Cancel();

  infobar_manager->RemoveInfoBar(infobar);
}

infobars::ContentInfoBarManager*
PPAPITestBase::InfoBarObserver::GetInfoBarManager() {
  content::WebContents* web_contents =
      test_base_->browser()->tab_strip_model()->GetActiveWebContents();
  return infobars::ContentInfoBarManager::FromWebContents(web_contents);
}

PPAPITestBase::PPAPITestBase() {
  // These are needed to test that the right NetworkAnonymizationKey is used.
  scoped_feature_list_.InitWithFeatures(
      // enabled_features
      {net::features::kPartitionConnectionsByNetworkIsolationKey, kNaclAllow},
      // disabled_features
      {});
}

void PPAPITestBase::SetUp() {
  base::FilePath document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&document_root));
  embedded_test_server()->AddDefaultHandlers(document_root);
  ASSERT_TRUE(embedded_test_server()->Start());

  EnablePixelOutput();
  InProcessBrowserTest::SetUp();
}

void PPAPITestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // Some stuff is hung off of the testing interface which is not enabled
  // by default.
  command_line->AppendSwitch(switches::kEnablePepperTesting);

  // Smooth scrolling confuses the scrollbar test.
  command_line->AppendSwitch(switches::kDisableSmoothScrolling);
}

void PPAPITestBase::SetUpOnMainThread() {
  host_resolver()->AddRuleWithFlags(
      "host_resolver.test", embedded_test_server()->host_port_pair().host(),
      net::HOST_RESOLVER_CANONNAME);
}

GURL PPAPITestBase::GetTestFileUrl(const std::string& test_case) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_path));
  test_path = test_path.Append(FILE_PATH_LITERAL("ppapi"));
  test_path = test_path.Append(FILE_PATH_LITERAL("tests"));
  test_path = test_path.Append(FILE_PATH_LITERAL("test_case.html"));

  // Sanity check the file name.
  EXPECT_TRUE(base::PathExists(test_path));

  GURL test_url = net::FilePathToFileURL(test_path);

  GURL::Replacements replacements;
  std::string query = BuildQuery(std::string(), test_case);
  replacements.SetQueryStr(query);
  return test_url.ReplaceComponents(replacements);
}

void PPAPITestBase::RunTest(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
}

void PPAPITestBase::RunTestViaHTTP(const std::string& test_case) {
  base::FilePath document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&document_root));
  RunTestURL(GetTestURL(*embedded_test_server(), test_case, std::string()));
}

void PPAPITestBase::RunTestWithSSLServer(const std::string& test_case) {
  base::FilePath http_document_root;
  ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&http_document_root));
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.AddDefaultHandlers(http_document_root);
  ASSERT_TRUE(ssl_server.Start());

  uint16_t port = ssl_server.host_port_pair().port();
  RunTestURL(GetTestURL(*embedded_test_server(), test_case,
                        base::StringPrintf("ssl_server_port=%d", port)));
}

void PPAPITestBase::RunTestWithWebSocketServer(const std::string& test_case) {
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  std::string host = ws_server.host_port_pair().HostForURL();
  uint16_t port = ws_server.host_port_pair().port();
  RunTestURL(
      GetTestURL(*embedded_test_server(), test_case,
                 base::StringPrintf("websocket_host=%s&websocket_port=%d",
                                    host.c_str(), port)));
}

void PPAPITestBase::RunTestIfAudioOutputAvailable(
    const std::string& test_case) {
  RunTest(test_case);
}

void PPAPITestBase::RunTestViaHTTPIfAudioOutputAvailable(
    const std::string& test_case) {
  RunTestViaHTTP(test_case);
}

void PPAPITestBase::RunTestURL(const GURL& test_url) {
  // See comment above TestingInstance in ppapi/test/testing_instance.h.
  // Basically it sends messages using the DOM automation controller. The
  // value of "..." means it's still working and we should continue to wait,
  // any other value indicates completion (in this case it will start with
  // "PASS" or "FAIL"). This keeps us from timing out on waits for long tests.
  PPAPITestMessageHandler handler;
  content::JavascriptTestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      &handler);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("PASS", handler.message().c_str());
}

GURL PPAPITestBase::GetTestURL(const net::EmbeddedTestServer& http_server,
                               const std::string& test_case,
                               const std::string& extra_params) {
  std::string query = BuildQuery("/test_case.html?", test_case);
  if (!extra_params.empty())
    query = base::StringPrintf("%s&%s", query.c_str(), extra_params.c_str());

  return http_server.GetURL(query);
}

PPAPITest::PPAPITest() : in_process_(true) {
}

void PPAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  // library name = "PPAPI Tests"
  // version = "1.2.3"
  base::FilePath::StringType parameters;
  parameters.append(FILE_PATH_LITERAL("#PPAPI Tests"));
  parameters.append(FILE_PATH_LITERAL("#"));  // No description.
  parameters.append(FILE_PATH_LITERAL("#1.2.3"));
  ASSERT_TRUE(
      ppapi::RegisterTestPluginWithExtraParameters(command_line, parameters));

  command_line->AppendSwitchASCII(switches::kAllowNaClSocketAPI, "127.0.0.1");
  if (in_process_)
    command_line->AppendSwitch(switches::kPpapiInProcess);

  // TODO(crbug.com/40166667): Remove once NaCl code can be deleted.
  command_line->AppendSwitchASCII(blink::switches::kBlinkSettings,
                                  "allowNonEmptyNavigatorPlugins=true");
}

std::string PPAPITest::BuildQuery(const std::string& base,
                                  const std::string& test_case){
  return base::StringPrintf("%stestcase=%s", base.c_str(), test_case.c_str());
}

void PPAPIPrivateTest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

OutOfProcessPPAPITest::OutOfProcessPPAPITest() {
  in_process_ = false;
}

void OutOfProcessPPAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
}

// Send touch events to a plugin and expect the events to reach the renderer
void OutOfProcessPPAPITest::RunTouchEventTest(const std::string& test_case) {
  RunTest(test_case);
  blink::SyntheticWebTouchEvent touchEvent;
  touchEvent.PressPoint(5, 5);
  RenderViewHost* rvh = browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetPrimaryMainFrame()
                            ->GetRenderViewHost();
  auto watcher = content::RenderViewHostTester::CreateInputWatcher(
      rvh, blink::WebInputEvent::Type::kTouchStart);

  // Wait for changes to be visible on the screen so that the touch event
  // can be consumed by the renderer.
  base::RunLoop run_loop;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetPrimaryMainFrame()
      ->InsertVisualStateCallback(base::BindOnce(
          [](base::OnceClosure quit_closure, bool result) {
            EXPECT_TRUE(result);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  content::RenderViewHostTester::SendTouchEvent(rvh, &touchEvent);
  touchEvent.ResetPoints();

  blink::mojom::InputEventResultState result = watcher->WaitForAck();
  blink::mojom::InputEventResultSource source =
      watcher->last_event_ack_source();

  // Verify the event reaches the renderer and an ack is received
  EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed, result);
  EXPECT_NE(blink::mojom::InputEventResultSource::kBrowser, source);

  touchEvent.ReleasePoint(0);
  watcher = content::RenderViewHostTester::CreateInputWatcher(
      rvh, blink::WebInputEvent::Type::kTouchEnd);
  content::RenderViewHostTester::SendTouchEvent(rvh, &touchEvent);
  watcher->WaitForAck();
}

void OutOfProcessPPAPIPrivateTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  OutOfProcessPPAPITest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

void PPAPINaClTest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);
#if BUILDFLAG(ENABLE_NACL)
  // Enable running (non-portable) NaCl outside of the Chrome web store.
  command_line->AppendSwitch(switches::kEnableNaCl);
  command_line->AppendSwitchASCII(switches::kAllowNaClSocketAPI, "127.0.0.1");
  command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
#endif
}

void PPAPINaClTest::RunTest(const std::string& test_case) {
#if BUILDFLAG(ENABLE_NACL)
  PPAPITestBase::RunTest(test_case);
#endif
}

void PPAPINaClTest::RunTestViaHTTP(const std::string& test_case) {
#if BUILDFLAG(ENABLE_NACL)
  PPAPITestBase::RunTestViaHTTP(test_case);
#endif
}

void PPAPINaClTest::RunTestWithSSLServer(const std::string& test_case) {
#if BUILDFLAG(ENABLE_NACL)
  PPAPITestBase::RunTestWithSSLServer(test_case);
#endif
}

void PPAPINaClTest::RunTestWithWebSocketServer(const std::string& test_case) {
#if BUILDFLAG(ENABLE_NACL)
  PPAPITestBase::RunTestWithWebSocketServer(test_case);
#endif
}

void PPAPINaClTest::RunTestIfAudioOutputAvailable(
    const std::string& test_case) {
#if BUILDFLAG(ENABLE_NACL)
  PPAPITestBase::RunTestIfAudioOutputAvailable(test_case);
#endif
}

void PPAPINaClTest::RunTestViaHTTPIfAudioOutputAvailable(
    const std::string& test_case) {
#if BUILDFLAG(ENABLE_NACL)
  PPAPITestBase::RunTestViaHTTPIfAudioOutputAvailable(test_case);
#endif
}

// Append the correct mode and testcase string
std::string PPAPINaClNewlibTest::BuildQuery(const std::string& base,
                                            const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_newlib&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIPrivateNaClNewlibTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClNewlibTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

// Append the correct mode and testcase string
std::string PPAPINaClGLibcTest::BuildQuery(const std::string& base,
                                           const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_glibc&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIPrivateNaClGLibcTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClGLibcTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

// Append the correct mode and testcase string
std::string PPAPINaClPNaClTest::BuildQuery(const std::string& base,
                                           const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_pnacl&testcase=%s", base.c_str(),
                            test_case.c_str());
}

void PPAPIPrivateNaClPNaClTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPINaClPNaClTest::SetUpCommandLine(command_line);
  AddPrivateSwitches(command_line);
}

void PPAPINaClTestDisallowedSockets::SetUpCommandLine(
    base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);

  // Enable running (non-portable) NaCl outside of the Chrome web store.
  command_line->AppendSwitch(switches::kEnableNaCl);
}

// Append the correct mode and testcase string
std::string PPAPINaClTestDisallowedSockets::BuildQuery(
    const std::string& base,
    const std::string& test_case) {
  return base::StringPrintf("%smode=nacl_newlib&testcase=%s", base.c_str(),
                            test_case.c_str());
}
