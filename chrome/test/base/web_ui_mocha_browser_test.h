// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WEB_UI_MOCHA_BROWSER_TEST_H_
#define CHROME_TEST_BASE_WEB_UI_MOCHA_BROWSER_TEST_H_

#include <string>

#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/in_process_browser_test.h"

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
  // - requires_focus: Whether to focus the web contents before running the
  //                   test, used for tests running as interactive_ui_tests.
  //                   Defaults to false.
  // - skip_test_loader: Whether to skip loading the test from
  //                     chrome://<test_loader_host_>/test_loader.html and load
  //                     it directly from chrome://<test_loader_host>. Defaults
  //                     to false.
  virtual void RunTest(const std::string& file,
                       const std::string& trigger,
                       const bool& requires_focus,
                       const bool& skip_test_loader);

  // Convenience overloaded version of the RunTest above, which uses the default
  // values for `requires_focus` and `skip_test_loader` (both false).
  virtual void RunTest(const std::string& file, const std::string& trigger);

  // Convenience overloaded version of the RunTest above, which uses the default
  // value for `requires_focus` and `skip_test_loader=true`.
  virtual void RunTestWithoutTestLoader(const std::string& file,
                                        const std::string& trigger);

  // InProcessBrowserTest overrides.
  void SetUpOnMainThread() override;

  void set_test_loader_host(const std::string& host);
  void set_requires_web_contents_focus_(const bool& value);

 private:
  // The host to use when invoking the test loader URL, like
  // "chrome://<host>/test_loader.html=...". Defaults to
  // `chrome::kChromeUIWebUITestHost`.
  // Note: It is also used by RunTest even when |skip_test_loader| is true.
  std::string test_loader_host_;

  // Handles collection of code coverage.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;
};

// Inherit from this class to explicitly focus the web contents before running
// any Mocha tests that exercise focus (necessary for Mac, see
// https://crbug.com/642467). This should only be used when running as part of
// interactive_ui_tests, and not as part of browser_tests.
class WebUIMochaFocusTest : public WebUIMochaBrowserTest {
 protected:
  void RunTest(const std::string& file, const std::string& trigger) override;
};

#endif  // CHROME_TEST_BASE_WEB_UI_MOCHA_BROWSER_TEST_H_
