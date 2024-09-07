// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_WEB_UI_BROWSER_TEST_H_
#define CHROME_TEST_BASE_ASH_WEB_UI_BROWSER_TEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/ash/javascript_browser_test.h"
#include "chrome/test/base/ash/web_ui_test_handler.h"
#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/devtools_listener.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"

namespace {
class WebUITestMessageHandler;
}

namespace base {
class Value;
}  // namespace base

namespace content {
class RenderFrameHost;
class ScopedWebUIControllerFactoryRegistration;
class WebUI;
struct GlobalRenderFrameHostId;
}  // namespace content

class TestChromeWebUIControllerFactory;

// The runner of WebUI javascript based tests.
// See chrome/test/data/webui/test_api.js for the javascript side test API's.
//
// These tests should follow the form given in:
// chrome/test/data/webui/sample_downloads.js.
// and the lone test within this class.
class BaseWebUIBrowserTest : public JavaScriptBrowserTest {
 public:
  ~BaseWebUIBrowserTest() override;

  // Runs a javascript function in the context of all libraries.
  // Note that calls to functions in test_api.js are not supported.
  // Takes ownership of Value* arguments.
  bool RunJavascriptFunction(const std::string& function_name);
  bool RunJavascriptFunction(const std::string& function_name, base::Value arg);
  bool RunJavascriptFunction(const std::string& function_name,
                             base::Value arg1,
                             base::Value arg2);
  bool RunJavascriptFunction(const std::string& function_name,
                             base::Value::List function_arguments);

  // Runs a test fixture that may include calls to functions in test_api.js.
  bool RunJavascriptTestF(bool is_async,
                          const std::string& test_fixture,
                          const std::string& test_name);

  // Runs a test that may include calls to functions in test_api.js.
  // Takes ownership of Value* arguments.
  bool RunJavascriptTest(const std::string& test_name);
  bool RunJavascriptTest(const std::string& test_name, base::Value arg);
  bool RunJavascriptTest(const std::string& test_name,
                         base::Value arg1,
                         base::Value arg2);
  bool RunJavascriptTest(const std::string& test_name,
                         base::Value::List test_arguments);

  // Runs a test that may include calls to functions in test_api.js, and waits
  // for call to testDone().  Takes ownership of Value* arguments.
  bool RunJavascriptAsyncTest(const std::string& test_name);
  bool RunJavascriptAsyncTest(const std::string& test_name, base::Value arg);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              base::Value arg1,
                              base::Value arg2);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              base::Value arg1,
                              base::Value arg2,
                              base::Value arg3);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              base::Value::List test_arguments);

  // Sends message through |preload_frame| to preload javascript libraries and
  // sets the |libraries_preloaded| flag to prevent re-loading at next
  // javascript invocation.
  void PreLoadJavascriptLibraries(const std::string& preload_test_fixture,
                                  const std::string& preload_test_name,
                                  content::RenderFrameHost* preload_frame);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  virtual void BrowsePreload(const GURL& browse_to);

 protected:
  // URL to dummy WebUI page for testing framework.
  static const std::string kDummyURL;

  BaseWebUIBrowserTest();

  // Accessors for preload test fixture and name.
  void set_preload_test_fixture(const std::string& preload_test_fixture);
  void set_preload_test_name(const std::string& preload_test_name);

  void set_webui_host(const std::string& webui_host);

  // Set up & tear down console error catching.
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Set a WebUI instance to run tests on.
  void SetWebUIInstance(content::WebUI* web_ui);

  // Returns a mock WebUI object under test (if any).
  virtual content::WebUIMessageHandler* GetMockMessageHandler();

  content::WebUI* override_selected_web_ui() {
    return override_selected_web_ui_;
  }

  // Returns a file:// GURL constructed from |path| inside the test data dir for
  // webui tests.
  static GURL WebUITestDataPathToURL(const base::FilePath::StringType& path);

  // Attaches mock and test handlers.
  virtual void SetupHandlers() = 0;

  WebUITestHandler* test_handler() { return test_handler_.get(); }
  void set_test_handler(std::unique_ptr<WebUITestHandler> test_handler) {
    test_handler_ = std::move(test_handler);
  }

  // Clears captured console error messages. Returns false if there were any.
  bool EnsureNoCapturedConsoleErrorMessages();

  // Handles collection of code coverage.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;

 private:
  // Loads all libraries added with AddLibrary(), and calls |function_name| with
  // |function_arguments|. When |is_test| is true, the framework wraps
  // |function_name| with a test helper function, which waits for completion,
  // logging an error message on failure, otherwise |function_name| is called
  // asynchronously. When |preload_frame| is non-NULL, sends the javascript to
  // the renderer frame for evaluation at the appropriate time before the onload
  // call is made. Passes |is_async| along to runTest wrapper.
  bool RunJavascriptUsingHandler(const std::string& function_name,
                                 base::Value::List function_arguments,
                                 bool is_test,
                                 bool is_async,
                                 content::RenderFrameHost* preload_frame);

  // Handles test framework messages.
  std::unique_ptr<WebUITestHandler> test_handler_;

  // Tracks the frames for which we've preloaded libraries.
  //
  // We use `GlobalRenderFrameHostId` because in certain cases, e.g. COOP/COEP,
  // the frame gets swapped during the navigation and we get two calls to
  // `WebContentsObserver::RenderFrameCreated()` (where we preload libraries).
  //
  // In the COOP/COEP case, `RenderFrameCreated()` is called for a speculative
  // RFH when the navigation starts, then at response time, after parsing the
  // headers, we realize that we need a new COOP/COEP-enabled SiteInstance, so
  // we'll create a different RFH for that SiteInstance and dispatch
  // `RenderFrameCreated()` for that RFH (and throw away the old speculative
  // RFH).
  std::set<content::GlobalRenderFrameHostId> libraries_preloaded_for_frames_;

  // Saves the states of |test_fixture| and |test_name| for calling
  // PreloadJavascriptLibraries().
  std::string preload_test_fixture_;
  std::string preload_test_name_;

  // When this is non-NULL, this is The WebUI instance used for testing.
  // Otherwise the selected tab's web_ui is used.
  raw_ptr<content::WebUI, AcrossTasksDanglingUntriaged>
      override_selected_web_ui_ = nullptr;

  std::unique_ptr<TestChromeWebUIControllerFactory> test_factory_;
  std::unique_ptr<content::ScopedWebUIControllerFactoryRegistration>
      factory_registration_;
};

class WebUIBrowserTest : public BaseWebUIBrowserTest {
 public:
  WebUIBrowserTest();
  ~WebUIBrowserTest() override;

  void SetupHandlers() override;

  void CollectCoverage(const std::string& test_name);

 private:
  // Owned by |test_handler_| in BaseWebUIBrowserTest.
  const raw_ptr<WebUITestMessageHandler, DanglingUntriaged>
      test_message_handler_;
};

#endif  // CHROME_TEST_BASE_ASH_WEB_UI_BROWSER_TEST_H_
