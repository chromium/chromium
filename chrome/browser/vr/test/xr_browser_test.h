// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_XR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_XR_BROWSER_TEST_H_

#include <unordered_set>

#include "base/callback.h"
#include "base/environment.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/vr/test/conditional_skipping.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/vr/test/test_hook.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace vr {

// Base browser test class for running XR-related tests.
// This is essentially a C++ port of the way Android does similar tests in
// //chrome/android/javatests/src/.../browser/vr/XrTestFramework.java
// This must be subclassed for different XR features to handle the differences
// between APIs and different usecases of the same API.
class XrBrowserTestBase : public InProcessBrowserTest {
 public:
  static constexpr base::TimeDelta kPollCheckIntervalShort =
      base::TimeDelta::FromMilliseconds(50);
  static constexpr base::TimeDelta kPollCheckIntervalLong =
      base::TimeDelta::FromMilliseconds(100);
  static constexpr base::TimeDelta kPollTimeoutShort =
      base::TimeDelta::FromMilliseconds(1000);
  static constexpr base::TimeDelta kPollTimeoutMedium =
      base::TimeDelta::FromMilliseconds(5000);
  static constexpr base::TimeDelta kPollTimeoutLong =
      base::TimeDelta::FromMilliseconds(10000);
  // Still considered XR-wide instead of VR-specific since OpenVR can be used
  // for passthrough AR with certain headsets.
  static constexpr char kVrOverrideEnvVar[] = "VR_OVERRIDE";
  static constexpr char kVrOverrideVal[] = "./mock_vr_clients/";
  static constexpr char kVrConfigPathEnvVar[] = "VR_CONFIG_PATH";
  static constexpr char kVrConfigPathVal[] = "./";
  static constexpr char kVrLogPathEnvVar[] = "VR_LOG_PATH";
  static constexpr char kVrLogPathVal[] = "./";
  static constexpr char kOpenXrConfigPathEnvVar[] = "XR_RUNTIME_JSON";
  static constexpr char kOpenXrConfigPathVal[] =
      "./mock_vr_clients/bin/openxr/openxr.json";
  static constexpr char kTestFileDir[] =
      "chrome/test/data/xr/e2e_test_files/html/";
  static constexpr char kSwitchIgnoreRuntimeRequirements[] =
      "ignore-runtime-requirements";
  static const std::vector<std::string> kRequiredTestSwitches;
  static const std::vector<std::pair<std::string, std::string>>
      kRequiredTestSwitchesWithValues;
  enum class TestStatus {
    STATUS_RUNNING = 0,
    STATUS_PASSED = 1,
    STATUS_FAILED = 2
  };

  enum class RuntimeType {
    RUNTIME_NONE = 0,
    RUNTIME_OPENVR = 1,
    RUNTIME_WMR = 2,
    RUNTIME_OPENXR = 3
  };

  XrBrowserTestBase();
  ~XrBrowserTestBase() override;

  void SetUp() override;
  void TearDown() override;

  virtual RuntimeType GetRuntimeType() const;

  // Returns a GURL to the XR test HTML file of the given name, e.g.
  // GetHtmlTestFile("foo") returns a GURL for the foo.html file in the XR
  // test HTML directory.
  GURL GetFileUrlForHtmlTestFile(const std::string& test_name);

  // Returns a GURL to the XR test HTML file of the given name served through
  // the local server.
  GURL GetEmbeddedServerUrlForHtmlTestFile(const std::string& test_name);

  // Returns a pointer to the embedded test server capable of serving test
  // HTML files, initializing and starting the server if necessary.
  net::EmbeddedTestServer* GetEmbeddedServer();

  // Convenience function for accessing the WebContents belonging to the current
  // tab open in the browser.
  content::WebContents* GetCurrentWebContents();

  // Loads the given GURL and blocks until the JavaScript on the page has
  // signalled that pre-test initialization is complete.
  void LoadUrlAndAwaitInitialization(const GURL& url);

  // Convenience function for ensuring the given JavaScript runs successfully
  // without having to always surround in ASSERT_TRUE.
  void RunJavaScriptOrFail(const std::string& js_expression,
                           content::WebContents* web_contents);

  // Convenience function for ensuring ExecuteScriptAndExtractBool runs
  // successfully and for directly getting the result instead of needing to pass
  // a pointer to be filled.
  bool RunJavaScriptAndExtractBoolOrFail(const std::string& js_expression,
                                         content::WebContents* web_contents);

  // Convenience function for ensuring ExecuteScripteAndExtractString runs
  // successfully and for directly getting the result instead of needing to pass
  // a pointer to be filled.
  std::string RunJavaScriptAndExtractStringOrFail(
      const std::string& js_expression,
      content::WebContents* web_contents);

  // Blocks until the given JavaScript expression evaluates to true or the
  // timeout is reached. Returns true if the expression evaluated to true or
  // false on timeout.
  bool PollJavaScriptBoolean(const std::string& bool_expression,
                             const base::TimeDelta& timeout,
                             content::WebContents* web_contents);

  // Polls the provided JavaScript boolean expression, failing the test if it
  // does not evaluate to true within the provided timeout.
  void PollJavaScriptBooleanOrFail(const std::string& bool_expression,
                                   const base::TimeDelta& timeout,
                                   content::WebContents* web_contents);

  // Blocks until the given callback returns true or the timeout is reached.
  // Fills the given bool with true if the condition successfully resolved or
  // false on timeout.
  void BlockOnCondition(base::RepeatingCallback<bool()> condition,
                        bool* result,
                        base::RunLoop* wait_loop,
                        const base::Time& start_time,
                        const base::TimeDelta& timeout = kPollTimeoutLong,
                        const base::TimeDelta& period = kPollCheckIntervalLong);

  // Blocks until the JavaScript in the given WebContents signals that it is
  // finished.
  void WaitOnJavaScriptStep(content::WebContents* web_contents);

  // Executes the given step/JavaScript expression and blocks until JavaScript
  // signals that it is finished.
  void ExecuteStepAndWait(const std::string& step_function,
                          content::WebContents* web_contents);

  // Retrieves the current status of the JavaScript test and returns an enum
  // corresponding to it.
  TestStatus CheckTestStatus(content::WebContents* web_contents);

  // Asserts that the JavaScript test code completed successfully.
  void EndTest(content::WebContents* web_contents);

  // Asserts that the JavaScript test harness did not detect any failures.
  // Similar to EndTest, but does not fail if the test is still detected as
  // running. This is useful because not all tests make use of the test harness'
  // test/assert features, but may still want to ensure that no unexpected
  // JavaScript errors were encountered.
  void AssertNoJavaScriptErrors(content::WebContents* web_contents);

  Browser* browser() {
    return browser_ == nullptr ? InProcessBrowserTest::browser() : browser_;
  }

  void SetBrowser(Browser* browser) { browser_ = browser; }

  Browser* CreateIncognitoBrowser(Profile* profile = nullptr) {
    return InProcessBrowserTest::CreateIncognitoBrowser(profile);
  }

  // Convenience function for running RunJavaScriptOrFail with the return value
  // of GetCurrentWebContents.
  void RunJavaScriptOrFail(const std::string& js_expression);

  // Convenience function for running RunJavaScriptAndExtractBoolOrFail with the
  // return value of GetCurrentWebContents.
  bool RunJavaScriptAndExtractBoolOrFail(const std::string& js_expression);

  // Convenience function for running RunJavaScriptAndExtractStringOrFail with
  // the return value of GetCurrentWebContents.
  std::string RunJavaScriptAndExtractStringOrFail(
      const std::string& js_expression);

  // Convenience function for running PollJavaScriptBoolean with the return
  // value of GetCurrentWebContents.
  bool PollJavaScriptBoolean(const std::string& bool_expression,
                             const base::TimeDelta& timeout);

  // Convenience function for running PollJavaScriptBooleanOrFail with the
  // return value of GetCurrentWebContents.
  void PollJavaScriptBooleanOrFail(
      const std::string& bool_expression,
      const base::TimeDelta& timeout = kPollTimeoutShort);

  // Convenience function for running WaitOnJavaScriptStep with the return value
  // of GetCurrentWebContents.
  void WaitOnJavaScriptStep();

  // Convenience function for running ExecuteStepAndWait with the return value
  // of GetCurrentWebContents.
  void ExecuteStepAndWait(const std::string& step_function);

  // Convenience function for running EndTest with the return value of
  // GetCurrentWebContents.
  void EndTest();

  // Convenience function for running AssertNoJavaScriptErrors with the return
  // value of GetCurrentWebContents.
  void AssertNoJavaScriptErrors();

  // Returns the set of runtime requirements to ignore, i.e. if a requirement
  // is in the vector, tests should not be skipped even if they don't meet the
  // requirement.
  std::unordered_set<std::string> GetIgnoredRuntimeRequirements() {
    return ignored_requirements_;
  }

 protected:
  std::unique_ptr<base::Environment> env_;
  std::vector<base::Feature> enable_features_;
  std::vector<base::Feature> disable_features_;
  std::vector<std::string> append_switches_;
  std::vector<std::string> enable_blink_features_;
  std::vector<XrTestRequirement> runtime_requirements_;
  std::unordered_set<std::string> ignored_requirements_;

#if defined(OS_WIN)
  HWND hwnd_;
#endif

 private:
  void LogJavaScriptFailure();
  Browser* browser_ = nullptr;
  std::unique_ptr<net::EmbeddedTestServer> server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  bool test_skipped_at_startup_ = false;
  bool javascript_failed_ = false;
  DISALLOW_COPY_AND_ASSIGN(XrBrowserTestBase);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_XR_BROWSER_TEST_H_
