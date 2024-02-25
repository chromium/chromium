// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WEB_UI_MOCHA_BROWSER_TEST_H_
#define CHROME_TEST_BASE_WEB_UI_MOCHA_BROWSER_TEST_H_

#include <string>

#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}  // namespace content

// Inherit from this class to run WebUI tests that are using Mocha.
class WebUIMochaBrowserTest : public InProcessBrowserTest {
 public:
  WebUIMochaBrowserTest(const WebUIMochaBrowserTest&) = delete;
  WebUIMochaBrowserTest& operator=(const WebUIMochaBrowserTest&) = delete;
  ~WebUIMochaBrowserTest() override;

 protected:
  WebUIMochaBrowserTest();

  // Runs the specified test.
  // - file: The module file holding the Mocha test.
  // - trigger: A JS string used to trigger the tests, defaults to
  //            "mocha.run()".
  // - skip_test_loader: Whether to skip loading the test from
  //                     chrome://<test_loader_host_>/test_loader.html and load
  //                     it directly from chrome://<test_loader_host>. Defaults
  //                     to false.
  void RunTest(const std::string& file,
               const std::string& trigger,
               const bool& skip_test_loader);

  // Convenience overloaded version of the RunTest above, which uses the default
  // value for `skip_test_loader`.
  void RunTest(const std::string& file, const std::string& trigger);

  // Convenience overloaded version of the RunTest above, which uses
  // `skip_test_loader=true`.
  void RunTestWithoutTestLoader(const std::string& file,
                                const std::string& trigger);

  // Similar to RunTest() but also accepts WebContents instance, for
  // cases where the test loads the WebContents to be tested in an
  // unconventional way.
  // Note: Unlike other RunTestXYZ methods above, this method does not
  // internally call FAIL on test failure, instead it returns an AssertionResult
  // that needs to be manually checked by callers.
  testing::AssertionResult RunTestOnWebContents(
      content::WebContents* web_contents,
      const std::string& file,
      const std::string& trigger,
      const bool& skip_test_loader);

  // Hook for subclasses that need to perform additional setup steps that
  // involve the WebContents, before the Mocha test runs.
  virtual void OnWebContentsAvailable(content::WebContents* web_contents);

  // Gets the WebContents instance to set up the chrome://webui-test data
  // source for. Defaults to chrome_test_utils::GetActiveWebContents(this);
  virtual content::WebContents* GetWebContentsForSetup();

  // InProcessBrowserTest overrides.
  void SetUpOnMainThread() override;

  void set_test_loader_host(const std::string& host);
  void set_test_loader_scheme(const std::string& scheme);

 private:
  // Helper that performs setup steps normally done by test_loader.html, invoked
  // in tests that don't use test_loader.html. Specifically:
  //  1) Programmatically loads mocha.js and mocha_adapter_simple.js.
  //  2) Programmatically loads the Mocha test file.
  testing::AssertionResult SimulateTestLoader(
      content::WebContents* web_contents,
      const std::string& file);

  // The host to use when invoking the test loader URL, like
  // "chrome://<host>/test_loader.html=...". Defaults to
  // `chrome::kChromeUIWebUITestHost`.
  // Note: It is also used by RunTest even when |skip_test_loader| is true.
  std::string test_loader_host_;

  // The scheme to use when invoking the test_loader URL, like
  // "<scheme>://webui-test/test_loader.html=...". Defaults to
  // content::kChromeUIScheme.
  // Note: It is also used by RunTest even when |skip_test_loader| is true.
  std::string test_loader_scheme_;

  // Handles collection of code coverage.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;
};

// Inherit from this class to explicitly focus the web contents before running
// any Mocha tests that exercise focus (necessary for Mac, see
// https://crbug.com/642467). This should only be used when running as part of
// interactive_ui_tests, and not as part of browser_tests.
class WebUIMochaFocusTest : public WebUIMochaBrowserTest {
 protected:
  void OnWebContentsAvailable(content::WebContents* web_contents) override;
};

#endif  // CHROME_TEST_BASE_WEB_UI_MOCHA_BROWSER_TEST_H_
