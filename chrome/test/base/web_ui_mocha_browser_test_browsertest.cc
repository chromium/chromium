// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

// Test that code coverage metrics are reported from WebUIMochaBrowserTest
// subclasses.
class WebUIMochaCoverageTest : public WebUIMochaBrowserTest {
 protected:
  base::ScopedTempDir tmp_dir_;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CHECK(tmp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kDevtoolsCodeCoverage,
                                   tmp_dir_.GetPath());
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WebUIMochaCoverageTest, TestCoverageEmits) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::IsDirectoryEmpty(tmp_dir_.GetPath()));

  // The actual WebUI test file used below does not matter here, as long as it
  // succeeds. Using it as a decoy to trigger the code coverage reporting.
  RunTest("js/test_suite_self_test.js",
          "mocha.fgrep('TestSuiteSelfTest Success').run();");

  // Scripts and tests are special directories under the WebUI specific
  // directory, ensure they have been created and are not empty.
  ASSERT_FALSE(base::IsDirectoryEmpty(tmp_dir_.GetPath()));
  base::FilePath coverage_dir =
      tmp_dir_.GetPath().AppendASCII("webui_javascript_code_coverage");
  ASSERT_FALSE(base::IsDirectoryEmpty(coverage_dir.AppendASCII("scripts")));
  ASSERT_FALSE(base::IsDirectoryEmpty(coverage_dir.AppendASCII("tests")));
}

// Test that various cases of errors or success are correctly detected.
class WebUIMochaSuccessFailureTest : public WebUIMochaBrowserTest {
 public:
  WebUIMochaSuccessFailureTest() {
    EXPECT_FALSE(s_test_);
    s_test_ = this;
  }

 protected:
  ~WebUIMochaSuccessFailureTest() override {
    EXPECT_TRUE(s_test_);
    s_test_ = nullptr;
  }

  static void RunTestStatic(const std::string& file,
                            const std::string& trigger) {
    ASSERT_TRUE(s_test_);
    s_test_->RunTest(file, trigger);
  }

  static void RunTestWithoutTestLoaderStatic(const std::string& file,
                                             const std::string& trigger) {
    ASSERT_TRUE(s_test_);
    s_test_->RunTestWithoutTestLoader(file, trigger);
  }

 private:
  // According to the interface for EXPECT_FATAL_FAILURE
  // (https://github.com/google/googletest/blob/main/docs/advanced.md#catching-failures)
  // the statement must be statically available. Therefore, we make a static
  // global s_test_ which should point to |this| for the duration of the test
  // run and be cleared afterward.
  static WebUIMochaSuccessFailureTest* s_test_;
};

WebUIMochaSuccessFailureTest* WebUIMochaSuccessFailureTest::s_test_ = nullptr;

// Test that when the script injected to trigger the Mocha tests contains an
// error, the test fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureTest, TriggerErrorFails) {
  EXPECT_FATAL_FAILURE(
      RunTestStatic("js/test_suite_self_test.js", "mmmmocha.run();"),
      "ReferenceError: mmmmocha is not defined");
}

// Test that when the requested host does not exist the test fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureTest, HostErrorFails) {
  set_test_loader_host("does-not-exist");
  EXPECT_FATAL_FAILURE(
      RunTestStatic("js/test_suite_self_test.js", "mocha.run();"),
      "Navigation to "
      "'chrome://does-not-exist/"
      "test_loader.html?adapter=mocha_adapter_simple.js&"
      "module=js/test_suite_self_test.js' failed.");
}

// Test that when the requested test file does not exist the test fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureTest, TestFileErrorFails) {
  EXPECT_FATAL_FAILURE(RunTestStatic("does_not_exist.js", "mocha.run();"), "");
}

// Test that when the underlying Mocha test fails, the C++ test also fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureTest, TestFailureFails) {
  EXPECT_FATAL_FAILURE(
      RunTestStatic("js/test_suite_self_test.js",
                    "mocha.fgrep('TestSuiteSelfTest Failure').run();"),
      "Mocha test failures detected in file: ");
}

// Test that when the underlying Mocha test succeeds, the C++ test also passes.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureTest, TestSuccessPasses) {
  RunTest("js/test_suite_self_test.js",
          "mocha.fgrep('TestSuiteSelfTest Success').run();");
}

// Test that various cases of errors or success are correctly detected when
// RunTestWithoutTestLoader() is used.
class WebUIMochaSuccessFailureWithoutTestLoaderTest
    : public WebUIMochaSuccessFailureTest {
 protected:
  WebUIMochaSuccessFailureWithoutTestLoaderTest() {
    // Pick a random WebUI host (but with the proper CSP headers) to run the
    // test from.
    set_test_loader_host(chrome::kChromeUIWebuiGalleryHost);
  }
};

// Test that when the script injected to trigger the Mocha tests contains an
// error, the test fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureWithoutTestLoaderTest,
                       TriggerErrorFails) {
  EXPECT_FATAL_FAILURE(RunTestWithoutTestLoaderStatic(
                           "js/test_suite_self_test.js", "mmmmocha.run();"),
                       "ReferenceError: mmmmocha is not defined");
}

// Test that when the requested host does not exist the test fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureWithoutTestLoaderTest,
                       HostErrorFails) {
  set_test_loader_host("does-not-exist");
  EXPECT_FATAL_FAILURE(RunTestWithoutTestLoaderStatic(
                           "js/test_suite_self_test.js", "mocha.run();"),
                       "Navigation to 'chrome://does-not-exist/' failed.");
}

// Test that when the requested test file does not exist the test fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureWithoutTestLoaderTest,
                       TestFileErrorFails) {
  EXPECT_FATAL_FAILURE(
      RunTestWithoutTestLoaderStatic("does_not_exist.js", "mocha.run();"),
      "TypeError: Failed to fetch dynamically imported module: "
      "chrome://webui-test/does_not_exist.js");
}

// Test that when the underlying Mocha test fails, the C++ test also fails.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureWithoutTestLoaderTest,
                       TestFailureFails) {
  EXPECT_FATAL_FAILURE(RunTestWithoutTestLoaderStatic(
                           "js/test_suite_self_test.js",
                           "mocha.fgrep('TestSuiteSelfTest Failure').run();"),
                       "Mocha test failures detected in file: ");
}

// Test that when the underlying Mocha test succeeds, the C++ test also passes.
IN_PROC_BROWSER_TEST_F(WebUIMochaSuccessFailureWithoutTestLoaderTest,
                       TestSuccessPasses) {
  RunTestWithoutTestLoader("js/test_suite_self_test.js",
                           "mocha.fgrep('TestSuiteSelfTest Success').run();");
}
