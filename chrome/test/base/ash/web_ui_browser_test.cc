// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/web_ui_browser_test.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ash/js_test_api.h"
#include "chrome/test/base/ash/web_ui_test_handler.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"

using content::RenderFrameHost;
using content::WebContents;
using content::WebUIController;
using content::WebUIMessageHandler;

namespace {

base::LazyInstance<std::vector<std::string>>::DestructorAtExit
    g_error_messages = LAZY_INSTANCE_INITIALIZER;

// Intercepts all log messages.
bool LogHandler(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str) {
  if (severity == logging::LOGGING_ERROR && file &&
      std::string("CONSOLE") == file) {
    g_error_messages.Get().push_back(str);
  }

  return false;
}

class WebUIJsInjectionReadyObserver : public content::WebContentsObserver {
 public:
  WebUIJsInjectionReadyObserver(content::WebContents* web_contents,
                                BaseWebUIBrowserTest* browser_test,
                                const std::string& preload_test_fixture,
                                const std::string& preload_test_name)
      : content::WebContentsObserver(web_contents),
        browser_test_(browser_test),
        preload_test_fixture_(preload_test_fixture),
        preload_test_name_(preload_test_name) {}

  void RenderFrameCreated(content::RenderFrameHost* frame_host) override {
    // We only load JS libraries in the main frame.
    if (!frame_host->GetParent()) {
      browser_test_->PreLoadJavascriptLibraries(preload_test_fixture_,
                                                preload_test_name_, frame_host);
    }
  }

 private:
  const raw_ptr<BaseWebUIBrowserTest> browser_test_;
  std::string preload_test_fixture_;
  std::string preload_test_name_;
};

// Handles chrome.send()-style test communication.
class WebUITestMessageHandler : public content::WebUIMessageHandler,
                                public WebUITestHandler {
 public:
  WebUITestMessageHandler() = default;
  WebUITestMessageHandler(const WebUITestMessageHandler&) = delete;
  WebUITestMessageHandler& operator=(const WebUITestMessageHandler&) = delete;
  ~WebUITestMessageHandler() override = default;

  // Receives testResult messages.
  void HandleTestResult(const base::Value::List& list) {
    // To ensure this gets done, do this before ASSERT* calls.
    RunQuitClosure();

    ASSERT_FALSE(list.empty());
    const bool test_succeeded = list[0].is_bool() && list[0].GetBool();
    std::string message;
    if (!test_succeeded) {
      ASSERT_EQ(2U, list.size());
      message = list[1].GetString();
    }

    TestComplete(test_succeeded ? std::optional<std::string>() : message);
  }

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "testResult",
        base::BindRepeating(&WebUITestMessageHandler::HandleTestResult,
                            base::Unretained(this)));
  }

  content::WebUI* GetWebUI() override { return web_ui(); }
};

}  // namespace

BaseWebUIBrowserTest::~BaseWebUIBrowserTest() = default;

bool BaseWebUIBrowserTest::RunJavascriptFunction(
    const std::string& function_name) {
  return RunJavascriptFunction(function_name, base::Value::List());
}

bool BaseWebUIBrowserTest::RunJavascriptFunction(
    const std::string& function_name,
    base::Value arg) {
  return RunJavascriptFunction(function_name,
                               base::Value::List().Append(std::move(arg)));
}

bool BaseWebUIBrowserTest::RunJavascriptFunction(
    const std::string& function_name,
    base::Value arg1,
    base::Value arg2) {
  return RunJavascriptFunction(
      function_name,
      base::Value::List().Append(std::move(arg1)).Append(std::move(arg2)));
}

bool BaseWebUIBrowserTest::RunJavascriptFunction(
    const std::string& function_name,
    base::Value::List function_arguments) {
  return RunJavascriptUsingHandler(function_name, std::move(function_arguments),
                                   false, false, nullptr);
}

bool BaseWebUIBrowserTest::RunJavascriptTestF(bool is_async,
                                              const std::string& test_fixture,
                                              const std::string& test_name) {
  auto args = base::Value::List().Append(test_fixture).Append(test_name);
  bool result = is_async ? RunJavascriptAsyncTest("RUN_TEST_F", std::move(args))
                         : RunJavascriptTest("RUN_TEST_F", std::move(args));

  if (coverage_handler_ && coverage_handler_->CoverageEnabled()) {
    const std::string& full_test_name = base::StrCat({test_fixture, test_name});
    coverage_handler_->CollectCoverage(full_test_name);
  }

  return result;
}

bool BaseWebUIBrowserTest::RunJavascriptTest(const std::string& test_name) {
  return RunJavascriptTest(test_name, base::Value::List());
}

bool BaseWebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                             base::Value arg) {
  return RunJavascriptTest(test_name,
                           base::Value::List().Append(std::move(arg)));
}

bool BaseWebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                             base::Value arg1,
                                             base::Value arg2) {
  return RunJavascriptTest(
      test_name,
      base::Value::List().Append(std::move(arg1)).Append(std::move(arg2)));
}

bool BaseWebUIBrowserTest::RunJavascriptTest(const std::string& test_name,
                                             base::Value::List test_arguments) {
  return RunJavascriptUsingHandler(test_name, std::move(test_arguments), true,
                                   false, nullptr);
}

bool BaseWebUIBrowserTest::RunJavascriptAsyncTest(
    const std::string& test_name) {
  return RunJavascriptAsyncTest(test_name, base::Value::List());
}

bool BaseWebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name,
                                                  base::Value arg) {
  return RunJavascriptAsyncTest(test_name,
                                base::Value::List().Append(std::move(arg)));
}

bool BaseWebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name,
                                                  base::Value arg1,
                                                  base::Value arg2) {
  return RunJavascriptAsyncTest(
      test_name,
      base::Value::List().Append(std::move(arg1)).Append(std::move(arg2)));
}

bool BaseWebUIBrowserTest::RunJavascriptAsyncTest(const std::string& test_name,
                                                  base::Value arg1,
                                                  base::Value arg2,
                                                  base::Value arg3) {
  return RunJavascriptAsyncTest(test_name, base::Value::List()
                                               .Append(std::move(arg1))
                                               .Append(std::move(arg2))
                                               .Append(std::move(arg3)));
}

bool BaseWebUIBrowserTest::RunJavascriptAsyncTest(
    const std::string& test_name,
    base::Value::List test_arguments) {
  return RunJavascriptUsingHandler(test_name, std::move(test_arguments), true,
                                   true, nullptr);
}

void BaseWebUIBrowserTest::PreLoadJavascriptLibraries(
    const std::string& preload_test_fixture,
    const std::string& preload_test_name,
    RenderFrameHost* preload_frame) {
  // We shouldn't preload libraries twice for the same frame in the same
  // process.
  auto global_frame_routing_id = preload_frame->GetGlobalId();
  ASSERT_FALSE(
      base::Contains(libraries_preloaded_for_frames_, global_frame_routing_id));

  RunJavascriptUsingHandler("preloadJavascriptLibraries",
                            base::Value::List()
                                .Append(preload_test_fixture)
                                .Append(preload_test_name),
                            false, false, preload_frame);
  libraries_preloaded_for_frames_.emplace(global_frame_routing_id);

  bool should_wait_flag = base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kWaitForDebuggerWebUI);

  if (should_wait_flag)
    RunJavascriptUsingHandler("setWaitUser", {}, false, false, preload_frame);
}

void BaseWebUIBrowserTest::BrowsePreload(const GURL& browse_to) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebUIJsInjectionReadyObserver injection_observer(
      web_contents, this, preload_test_fixture_, preload_test_name_);
  content::TestNavigationObserver navigation_observer(web_contents);

  NavigateParams params(browser(), browse_to, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;

  Navigate(&params);
  navigation_observer.Wait();
}

BaseWebUIBrowserTest::BaseWebUIBrowserTest() = default;

void BaseWebUIBrowserTest::set_preload_test_fixture(
    const std::string& preload_test_fixture) {
  preload_test_fixture_ = preload_test_fixture;
}

void BaseWebUIBrowserTest::set_preload_test_name(
    const std::string& preload_test_name) {
  preload_test_name_ = preload_test_name;
}

void BaseWebUIBrowserTest::set_webui_host(const std::string& webui_host) {
  test_factory_->set_webui_host(webui_host);
}

namespace {

const GURL& DummyUrl() {
  static GURL url(content::GetWebUIURLString("DummyURL"));
  return url;
}

// DataSource for the dummy URL.  If no data source is provided then an error
// page is shown. While this doesn't matter for most tests, without it,
// navigation to different anchors cannot be listened to (via the hashchange
// event).
class MockWebUIDataSource : public content::URLDataSource {
 public:
  MockWebUIDataSource() = default;
  MockWebUIDataSource(const MockWebUIDataSource&) = delete;
  MockWebUIDataSource& operator=(const MockWebUIDataSource&) = delete;
  ~MockWebUIDataSource() override = default;

 private:
  std::string GetSource() override { return "dummyurl"; }

  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override {
    scoped_refptr<base::RefCountedString> response =
        base::MakeRefCounted<base::RefCountedString>(
            std::string("<html><body>Dummy</body></html>"));
    std::move(callback).Run(response.get());
  }

  std::string GetMimeType(const GURL& url) override { return "text/html"; }

  std::string GetContentSecurityPolicy(
      const network::mojom::CSPDirectiveName directive) override {
    if (directive == network::mojom::CSPDirectiveName::ScriptSrc) {
      return "script-src chrome://resources chrome://webui-test 'self';";
    } else if (directive ==
                   network::mojom::CSPDirectiveName::RequireTrustedTypesFor ||
               directive == network::mojom::CSPDirectiveName::TrustedTypes) {
      return std::string();
    }

    return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
};

// WebUIProvider to allow attaching the DataSource for the dummy URL when
// testing.
class MockWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MockWebUIProvider() = default;
  MockWebUIProvider(const MockWebUIProvider&) = delete;
  MockWebUIProvider& operator=(const MockWebUIProvider&) = delete;
  ~MockWebUIProvider() override = default;

  // Returns a new WebUI
  std::unique_ptr<WebUIController> NewWebUI(content::WebUI* web_ui,
                                            const GURL& url) override {
    content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                                std::make_unique<MockWebUIDataSource>());
    return std::make_unique<content::WebUIController>(web_ui);
  }
};

base::LazyInstance<MockWebUIProvider>::DestructorAtExit mock_provider_ =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void BaseWebUIBrowserTest::SetUpOnMainThread() {
  JavaScriptBrowserTest::SetUpOnMainThread();

  base::FilePath pak_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::kScaleFactorNone);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
    base::FilePath devtools_code_coverage_dir =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
    coverage_handler_ = std::make_unique<DevToolsAgentCoverageObserver>(
        devtools_code_coverage_dir);
  }

  logging::SetLogMessageHandler(&LogHandler);

  if (crosapi::browser_util::IsLacrosEnabled() && browser() == nullptr) {
    // Create a new Ash browser window so test code using browser() can work
    // even when Lacros is the only browser.
    // TODO(crbug.com/40270051): Remove uses of browser() from such tests.
    chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
    SelectFirstBrowser();
  }

  // For tests that run on the login screen, there is no Browser during
  // SetUpOnMainThread() so skip adding the chrome://webui-test data source.
  // These tests don't need it anyway.
  if (browser()) {
    // Register data sources for chrome://webui-test/ URLs
    // e.g. `chrome://webui-test/chai_assert.js`.
    webui::CreateAndAddWebUITestDataSource(browser()->profile());
  }

  test_factory_ = std::make_unique<TestChromeWebUIControllerFactory>();
  factory_registration_ =
      std::make_unique<content::ScopedWebUIControllerFactoryRegistration>(
          test_factory_.get(), ChromeWebUIControllerFactory::GetInstance());

  test_factory_->AddFactoryOverride(DummyUrl().host(),
                                    mock_provider_.Pointer());
  test_factory_->AddFactoryOverride(content::kChromeUIResourcesHost,
                                    mock_provider_.Pointer());
}

void BaseWebUIBrowserTest::TearDownOnMainThread() {
  logging::SetLogMessageHandler(nullptr);

  test_factory_->RemoveFactoryOverride(DummyUrl().host());
  // |factory_registration_| must be reset before |test_factory_| to remove
  // any pointers to |test_factory_| from the factory registry before its
  // destruction.
  factory_registration_.reset();
  test_factory_.reset();
}

void BaseWebUIBrowserTest::SetWebUIInstance(content::WebUI* web_ui) {
  override_selected_web_ui_ = web_ui;
}

WebUIMessageHandler* BaseWebUIBrowserTest::GetMockMessageHandler() {
  return nullptr;
}

bool BaseWebUIBrowserTest::RunJavascriptUsingHandler(
    const std::string& function_name,
    base::Value::List function_arguments,
    bool is_test,
    bool is_async,
    RenderFrameHost* preload_frame) {
  if (!preload_frame)
    SetupHandlers();

  // Get the user libraries. Preloading them individually is best, then
  // we can assign each one a filename for better stack traces. Otherwise
  // append them all to |content|.
  std::u16string content;
  std::vector<std::u16string> libraries;

  // Some tests don't use `BaseWebUIBrowserTest::BrowsePreload()`, which is
  // where we attach WebUIJsInjectionReadyObserver and preload libraries. In
  // these cases prepend the libraries to the test itself.
  auto* frame_for_libraries = preload_frame
                                  ? preload_frame
                                  : test_handler_->GetRenderFrameHostForTest();
  if (!base::Contains(libraries_preloaded_for_frames_,
                      frame_for_libraries->GetGlobalId())) {
    BuildJavascriptLibraries(&libraries);
    if (!preload_frame) {
      content = base::JoinString(libraries, u"\n");
      libraries.clear();
    }
  }

  if (!function_name.empty()) {
    std::u16string called_function;
    if (is_test) {
      called_function = BuildRunTestJSCall(is_async, function_name,
                                           std::move(function_arguments));
    } else {
      called_function =
          content::WebUI::GetJavascriptCall(function_name, function_arguments);
    }
    content.append(called_function);
  }

  bool result = true;

  for (const std::u16string& library : libraries)
    test_handler_->PreloadJavaScript(library, preload_frame);

  if (is_test)
    result = test_handler_->RunJavaScriptTestWithResult(content);
  else if (preload_frame)
    test_handler_->PreloadJavaScript(content, preload_frame);
  else
    test_handler_->RunJavaScript(content);

  if (!EnsureNoCapturedConsoleErrorMessages()) {
    result = false;
  }

  return result;
}

bool BaseWebUIBrowserTest::EnsureNoCapturedConsoleErrorMessages() {
  if (g_error_messages.Get().empty()) {
    return true;
  }

  LOG(ERROR) << "CONDITION FAILURE: encountered javascript console error(s):";
  for (const auto& msg : g_error_messages.Get()) {
    LOG(ERROR) << "JS ERROR: '" << msg << "'";
  }
  LOG(ERROR) << "JS call assumed failed, because JS console error(s) found.";

  g_error_messages.Get().clear();
  return false;
}

GURL BaseWebUIBrowserTest::WebUITestDataPathToURL(
    const base::FilePath::StringType& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_path(JsTestApiConfig().search_path);
  EXPECT_TRUE(base::PathExists(test_path));
  return net::FilePathToFileURL(test_path);
}

WebUIBrowserTest::WebUIBrowserTest()
    : test_message_handler_(new WebUITestMessageHandler) {
  set_test_handler(std::unique_ptr<WebUITestHandler>(test_message_handler_));
}

WebUIBrowserTest::~WebUIBrowserTest() = default;

void WebUIBrowserTest::SetupHandlers() {
  content::WebUI* web_ui_instance =
      override_selected_web_ui()
          ? override_selected_web_ui()
          : browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI();
  ASSERT_TRUE(web_ui_instance);

  test_message_handler_->set_web_ui(web_ui_instance);
  test_message_handler_->RegisterMessages();

  if (GetMockMessageHandler()) {
    GetMockMessageHandler()->set_web_ui(web_ui_instance);
    GetMockMessageHandler()->RegisterMessages();
  }
}

void WebUIBrowserTest::CollectCoverage(const std::string& test_name) {
  return coverage_handler_->CollectCoverage(test_name);
}
