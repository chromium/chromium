// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WEB_UI_BROWSER_TEST_H_
#define CHROME_TEST_BASE_WEB_UI_BROWSER_TEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ui/webui/web_ui_test_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/javascript_browser_test.h"

namespace {
class WebUITestMessageHandler;
}

namespace base {
class Value;
}

namespace content {
class RenderViewHost;
class WebUI;
}

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
                             std::vector<base::Value> function_arguments);

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
                         std::vector<base::Value> test_arguments);

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
                              std::vector<base::Value> test_arguments);

  // Sends message through |preload_host| to preload javascript libraries and
  // sets the |libraries_preloaded| flag to prevent re-loading at next
  // javascript invocation.
  void PreLoadJavascriptLibraries(const std::string& preload_test_fixture,
                                  const std::string& preload_test_name,
                                  content::RenderViewHost* preload_host);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  virtual void BrowsePreload(const GURL& browse_to);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  void BrowsePrintPreload(const GURL& browse_to);

 protected:
  // URL to dummy WebUI page for testing framework.
  static const std::string kDummyURL;

  BaseWebUIBrowserTest();

  // Accessors for preload test fixture and name.
  void set_preload_test_fixture(const std::string& preload_test_fixture);
  void set_preload_test_name(const std::string& preload_test_name);

  void set_webui_host(const std::string& webui_host);

  // Enable command line flags for test.
  void SetUpCommandLine(base::CommandLine* command_line) override;

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

 private:
  // Loads all libraries added with AddLibrary(), and calls |function_name| with
  // |function_arguments|. When |is_test| is true, the framework wraps
  // |function_name| with a test helper function, which waits for completion,
  // logging an error message on failure, otherwise |function_name| is called
  // asynchronously. When |preload_host| is non-NULL, sends the javascript to
  // the RenderView for evaluation at the appropriate time before the onload
  // call is made. Passes |is_async| along to runTest wrapper.
  bool RunJavascriptUsingHandler(const std::string& function_name,
                                 std::vector<base::Value> function_arguments,
                                 bool is_test,
                                 bool is_async,
                                 content::RenderViewHost* preload_host);

  // Handles test framework messages.
  std::unique_ptr<WebUITestHandler> test_handler_;

  // Indicates that the libraries have been pre-loaded and to not load them
  // again.
  bool libraries_preloaded_;

  // Saves the states of |test_fixture| and |test_name| for calling
  // PreloadJavascriptLibraries().
  std::string preload_test_fixture_;
  std::string preload_test_name_;

  // When this is non-NULL, this is The WebUI instance used for testing.
  // Otherwise the selected tab's web_ui is used.
  content::WebUI* override_selected_web_ui_;

  std::unique_ptr<TestChromeWebUIControllerFactory> test_factory_;
};

class WebUIBrowserTest : public BaseWebUIBrowserTest {
 public:
  WebUIBrowserTest();
  ~WebUIBrowserTest() override;

  void SetupHandlers() override;

 private:
  WebUITestMessageHandler* test_message_handler_;
};

#endif  // CHROME_TEST_BASE_WEB_UI_BROWSER_TEST_H_
