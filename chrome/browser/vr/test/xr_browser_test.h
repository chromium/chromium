// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_XR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_XR_BROWSER_TEST_H_

#include "base/callback.h"
#include "base/environment.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

#define REQUIRES_GPU(x) DISABLED_##x

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
  enum class TestStatus {
    STATUS_RUNNING = 0,
    STATUS_PASSED = 1,
    STATUS_FAILED = 2
  };

  XrBrowserTestBase();
  ~XrBrowserTestBase() override;

  void SetUp() override;

  // Returns a GURL to the XR test HTML file of the given name, e.g.
  // GetHtmlTestFile("foo") returns a GURL for the foo.html file in the XR
  // test HTML directory.
  GURL GetHtmlTestFile(const std::string& test_name);

  // Convenience function for accessing the WebContents belonging to the first
  // tab open in the browser.
  content::WebContents* GetFirstTabWebContents();

  // Loads the given GURL and blocks until the JavaScript on the page has
  // signalled that pre-test initialization is complete.
  void LoadUrlAndAwaitInitialization(const GURL& url);

  // Convenience function for ensuring the given JavaScript runs successfully
  // without having to always surround in EXPECT_TRUE.
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
  // Returns true if the condition successfully resolved or false on timeout.
  // This is unsafe because it relies on the provided callback checking a result
  // from a different thread. This isn't an issue when blocking on some
  // JavaScript condition to be true, but could be problematic if forced into
  // use elsewhere.
  bool BlockOnConditionUnsafe(
      base::RepeatingCallback<bool()> condition,
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

  Browser* browser() { return InProcessBrowserTest::browser(); }

  // Convenience function for running RunJavaScriptOrFail with the return value
  // of GetFirstTabWebContents.
  void RunJavaScriptOrFail(const std::string& js_expression);

  // Convenience function for running RunJavaScriptAndExtractBoolOrFail with the
  // return value of GetFirstTabWebContents.
  bool RunJavaScriptAndExtractBoolOrFail(const std::string& js_expression);

  // Convenience function for running RunJavaScriptAndExtractStringOrFail with
  // the return value of GetFirstTabWebContents.
  std::string RunJavaScriptAndExtractStringOrFail(
      const std::string& js_expression);

  // Convenience function for running PollJavaScriptBoolean with the return
  // value of GetFirstTabWebContents.
  bool PollJavaScriptBoolean(const std::string& bool_expression,
                             const base::TimeDelta& timeout);

  // Convenience function for running PollJavaScriptBooleanOrFail with the
  // return value of GetFirstTabWebContents.
  void PollJavaScriptBooleanOrFail(const std::string& bool_expression,
                                   const base::TimeDelta& timeout);

  // Convenience function for running WaitOnJavaScriptStep with the return value
  // of GetFirstTabWebContents.
  void WaitOnJavaScriptStep();

  // Convenience function for running ExecuteStepAndWait with the return value
  // of GetFirstTabWebContents.
  void ExecuteStepAndWait(const std::string& step_function);

  // Convenience function for running EndTest with the return value of
  // GetFirstTabWebContents.
  void EndTest();

  // Convenience function for running AssertNoJavaScriptErrors with the return
  // value of GetFirstTabWebContents.
  void AssertNoJavaScriptErrors();

 protected:
  std::unique_ptr<base::Environment> env_;
  std::vector<base::Feature> enable_features_;
  std::vector<std::string> append_switches_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(XrBrowserTestBase);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_XR_BROWSER_TEST_H_
