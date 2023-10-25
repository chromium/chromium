// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/ash/web_ui_browser_test.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

using content::WebUIMessageHandler;

namespace {
const GURL& DummyUrl() {
  static GURL url(content::GetWebUIURLString("DummyURL"));
  return url;
}
}  // namespace

// According to the interface for EXPECT_FATAL_FAILURE
// (https://github.com/google/googletest/blob/main/docs/advanced.md#catching-failures)
// the statement must be statically available. Therefore, we make a static
// global s_test_ which should point to |this| for the duration of the test run
// and be cleared afterward.
class WebUIBrowserExpectFailTest : public WebUIBrowserTest {
 public:
  WebUIBrowserExpectFailTest() {
    EXPECT_FALSE(s_test_);
    s_test_ = this;
  }

 protected:
  ~WebUIBrowserExpectFailTest() override {
    EXPECT_TRUE(s_test_);
    s_test_ = nullptr;
  }

  static void RunJavascriptTestNoReturn(const std::string& testname) {
    EXPECT_TRUE(s_test_);
    s_test_->RunJavascriptTest(testname);
  }

  static void RunJavascriptAsyncTestNoReturn(const std::string& testname) {
    EXPECT_TRUE(s_test_);
    s_test_->RunJavascriptAsyncTest(testname);
  }

 private:
  static WebUIBrowserTest* s_test_;
};

WebUIBrowserTest* WebUIBrowserExpectFailTest::s_test_ = nullptr;

IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, TestFailsFast) {
  AddLibrary(base::FilePath(FILE_PATH_LITERAL("sample_downloads.js")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_FATAL_FAILURE(RunJavascriptTestNoReturn("DISABLED_BogusFunctionName"),
                       "result.is_bool()");
}

// Test that bogus javascript fails fast - no timeout waiting for result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, TestRuntimeErrorFailsFast) {
  AddLibrary(base::FilePath(FILE_PATH_LITERAL("runtime_error.js")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), DummyUrl()));
  EXPECT_FATAL_FAILURE(RunJavascriptTestNoReturn("TestRuntimeErrorFailsFast"),
                       "result.is_bool()");
}

// Test times out in debug builds: https://crbug.com/902310
#if !defined(NDEBUG)
#define MAYBE_TestFailsAsyncFast DISABLED_TestFailsAsyncFast
#else
#define MAYBE_TestFailsAsyncFast TestFailsAsyncFast
#endif

// Test that bogus javascript fails async test fast as well - no timeout waiting
// for result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserExpectFailTest, MAYBE_TestFailsAsyncFast) {
  AddLibrary(base::FilePath(FILE_PATH_LITERAL("sample_downloads.js")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_FATAL_FAILURE(
      RunJavascriptAsyncTestNoReturn("DISABLED_BogusFunctionName"),
      "result.is_bool()");
}

// Tests that the async framework works.
class WebUIBrowserAsyncTest : public WebUIBrowserTest {
 public:
  WebUIBrowserAsyncTest(const WebUIBrowserAsyncTest&) = delete;
  WebUIBrowserAsyncTest& operator=(const WebUIBrowserAsyncTest&) = delete;
  // Calls the testDone() function from test_api.js
  void TestDone() {
    RunJavascriptFunction("testDone");
  }

  // Starts a failing test.
  void RunTestFailsAssert() {
    RunJavascriptFunction("runAsync", base::Value("testFailsAssert"));
  }

  // Starts a passing test.
  void RunTestPasses() {
    RunJavascriptFunction("runAsync", base::Value("testPasses"));
  }

 protected:
  WebUIBrowserAsyncTest() {}

  // Class to synchronize asynchronous javascript activity with the tests.
  class AsyncWebUIMessageHandler : public WebUIMessageHandler {
   public:
    AsyncWebUIMessageHandler() = default;
    AsyncWebUIMessageHandler(const AsyncWebUIMessageHandler&) = delete;
    AsyncWebUIMessageHandler& operator=(const AsyncWebUIMessageHandler&) =
        delete;

    MOCK_METHOD1(HandleTestContinues, void(const base::Value::List&));
    MOCK_METHOD1(HandleTestFails, void(const base::Value::List&));
    MOCK_METHOD1(HandleTestPasses, void(const base::Value::List&));

   private:
    void RegisterMessages() override {
      web_ui()->RegisterMessageCallback(
          "startAsyncTest",
          base::BindRepeating(&AsyncWebUIMessageHandler::HandleStartAsyncTest,
                              base::Unretained(this)));
      web_ui()->RegisterMessageCallback(
          "testContinues",
          base::BindRepeating(&AsyncWebUIMessageHandler::HandleTestContinues,
                              base::Unretained(this)));
      web_ui()->RegisterMessageCallback(
          "testFails",
          base::BindRepeating(&AsyncWebUIMessageHandler::HandleTestFails,
                              base::Unretained(this)));
      web_ui()->RegisterMessageCallback(
          "testPasses",
          base::BindRepeating(&AsyncWebUIMessageHandler::HandleTestPasses,
                              base::Unretained(this)));
    }

    // Starts the test in |list_value|[0] with the runAsync wrapper.
    void HandleStartAsyncTest(const base::Value::List& list_value) {
      const base::Value& test_name = list_value[0];
      web_ui()->CallJavascriptFunctionUnsafe("runAsync", test_name);
    }
  };

  // Handler for this object.
  ::testing::StrictMock<AsyncWebUIMessageHandler> message_handler_;

 private:
  // Provide this object's handler.
  WebUIMessageHandler* GetMockMessageHandler() override {
    return &message_handler_;
  }

  // Set up and browse to DummyUrl() for all tests.
  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(base::FilePath(FILE_PATH_LITERAL("async.js")));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), DummyUrl()));
  }
};

// Test that assertions fail immediately after assertion fails (no testContinues
// message). (Sync version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestSyncOkTestFail) {
  ASSERT_FALSE(RunJavascriptTest("testFailsAssert"));
}

// Test that assertions fail immediately after assertion fails (no testContinues
// message). (Async version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncFailsAssert) {
  EXPECT_CALL(message_handler_, HandleTestFails(::testing::_));
  ASSERT_FALSE(
      RunJavascriptAsyncTest("startAsyncTest", base::Value("testFailsAssert")));
}

// Test that test continues and passes. (Sync version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestSyncPasses) {
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  ASSERT_TRUE(RunJavascriptTest("testPasses"));
}

// Test that test continues and passes. (Async version).
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncPasses) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::TestDone));
  ASSERT_TRUE(
      RunJavascriptAsyncTest("startAsyncTest", base::Value("testPasses")));
}

// Test that two tests pass.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncPassPass) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::RunTestPasses));
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::TestDone));
  ASSERT_TRUE(
      RunJavascriptAsyncTest("startAsyncTest", base::Value("testPasses")));
}

// Test that first test passes; second fails.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestAsyncPassThenFail) {
  ::testing::InSequence s;
  EXPECT_CALL(message_handler_, HandleTestContinues(::testing::_));
  EXPECT_CALL(message_handler_, HandleTestPasses(::testing::_))
      .WillOnce(::testing::InvokeWithoutArgs(
          this, &WebUIBrowserAsyncTest::RunTestFailsAssert));
  EXPECT_CALL(message_handler_, HandleTestFails(::testing::_));
  ASSERT_FALSE(
      RunJavascriptAsyncTest("startAsyncTest", base::Value("testPasses")));
}

// Test that calling testDone during RunJavascriptAsyncTest still completes
// when waiting for async result. This is similar to the previous test, but call
// testDone directly and expect pass result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestTestDoneEarlyPassesAsync) {
  ASSERT_TRUE(RunJavascriptAsyncTest("testDone"));
}

// Test that calling testDone during RunJavascriptTest still completes when
// waiting for async result.
IN_PROC_BROWSER_TEST_F(WebUIBrowserAsyncTest, TestTestDoneEarlyPasses) {
  ASSERT_TRUE(RunJavascriptTest("testDone"));
}

class WebUICoverageTest : public WebUIBrowserTest {
 protected:
  base::ScopedTempDir tmp_dir_;

 private:
  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(base::FilePath(FILE_PATH_LITERAL("async.js")));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), DummyUrl()));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CHECK(tmp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kDevtoolsCodeCoverage,
                                   tmp_dir_.GetPath());
    WebUIBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WebUICoverageTest, TestCoverageEmits) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::IsDirectoryEmpty(tmp_dir_.GetPath()));
  ASSERT_TRUE(RunJavascriptTest("testDone"));

  CollectCoverage("foo");
  ASSERT_FALSE(base::IsDirectoryEmpty(tmp_dir_.GetPath()));

  // Scripts and tests are special directories under the WebUI specific
  // directory, ensure they have been created and are not empty.
  base::FilePath coverage_dir =
      tmp_dir_.GetPath().AppendASCII("webui_javascript_code_coverage");
  ASSERT_FALSE(base::IsDirectoryEmpty(coverage_dir.AppendASCII("scripts")));
  ASSERT_FALSE(base::IsDirectoryEmpty(coverage_dir.AppendASCII("tests")));
}
