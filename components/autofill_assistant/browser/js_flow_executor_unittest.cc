// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_executor.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/switches.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::WithArg;

// Parses |json| as a base::Value. No error handling - this will crash for
// invalid json inputs.
std::unique_ptr<base::Value> UniqueValueFromJson(const std::string& json) {
  return std::make_unique<base::Value>(
      std::move(*base::JSONReader::Read(json)));
}

class MockJsFlowExecutorDelegate : public JsFlowExecutor::Delegate {
 public:
  MockJsFlowExecutorDelegate() = default;
  ~MockJsFlowExecutorDelegate() override = default;

  MOCK_METHOD(
      void,
      RunNativeAction,
      (std::unique_ptr<base::Value> native_action,
       base::OnceCallback<void(const ClientStatus& result_status,
                               std::unique_ptr<base::Value> result_value)>
           callback),
      (override));
};

class JsFlowExecutorTest : public content::ContentBrowserTest {
 public:
  JsFlowExecutorTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("site-per-process");
    // Necessary to avoid flakiness or failure due to input arriving
    // before the first compositor commit.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Start a mock server for hosting an OOPIF.
    http_server_iframe_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_iframe_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant/html_iframe");
    ASSERT_TRUE(http_server_iframe_->Start(8081));

    // Start the main server hosting the test page.
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant/html");
    ASSERT_TRUE(http_server_->Start(8080));
    ASSERT_TRUE(NavigateToURL(
        shell(),
        http_server_->GetURL("/autofill_assistant_target_website.html")));

    flow_executor_ = std::make_unique<JsFlowExecutor>(shell()->web_contents(),
                                                      &mock_delegate_);
  }

  // Overload, ignore result value, just return the client status.
  ClientStatus RunTest(const std::string& js_flow) {
    std::unique_ptr<base::Value> ignored_result;
    return RunTest(js_flow, ignored_result);
  }

  ClientStatus RunTest(const std::string& js_flow,
                       std::unique_ptr<base::Value>& result_value) {
    ClientStatus status;
    base::RunLoop run_loop;
    flow_executor_->Start(
        js_flow, base::BindOnce(&JsFlowExecutorTest::OnFlowFinished,
                                base::Unretained(this), run_loop.QuitClosure(),
                                &status, std::ref(result_value)));
    run_loop.Run();
    return status;
  }

  void OnFlowFinished(base::OnceClosure done_callback,
                      ClientStatus* status_output,
                      std::unique_ptr<base::Value>& result_output,
                      const ClientStatus& status,
                      std::unique_ptr<base::Value> result_value) {
    *status_output = status;
    result_output = std::move(result_value);
    std::move(done_callback).Run();
  }

 protected:
  NiceMock<MockJsFlowExecutorDelegate> mock_delegate_;
  std::unique_ptr<JsFlowExecutor> flow_executor_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_iframe_;
};

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, SmokeTest) {
  EXPECT_THAT(RunTest(std::string()),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, InvalidJs) {
  EXPECT_THAT(RunTest("Not valid Javascript"),
              Property(&ClientStatus::proto_status, UNEXPECTED_JS_ERROR));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, RunNativeActionWithReturnValue) {
  std::unique_ptr<base::Value> native_return_value =
      std::make_unique<base::Value>(std::move(*base::JSONReader::Read(
          R"(
          {
            "keyA":12345,
            "keyB":"Hello world",
            "keyC": ["array", "of", "strings"],
            "keyD": {
              "keyE": "nested",
              "keyF": true,
              "keyG": 123.45,
              "keyH": null
            }
          }
        )")));

  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](auto value, auto callback) {
        EXPECT_EQ(*value, *UniqueValueFromJson(R"(
          {"type":"string",
           "value":"test"})"));
        std::move(callback).Run(ClientStatus(ACTION_APPLIED),
                                std::move(native_return_value));
      });

  std::unique_ptr<base::Value> js_return_value;
  EXPECT_THAT(RunTest(R"(
                        let [status, value] = await runNativeAction('test');
                        if (status != 2) { // ACTION_APPLIED
                          return status;
                        }
                        value.keyA += 3;
                        value.keyB += '!';
                        value.keyD.keyF = false;
                        return value;
                      )",
                      js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*js_return_value, *base::JSONReader::Read(R"(
    {
       "result": {
          "type": "object",
          "value": {
             "keyA": 12348,
             "keyB": "Hello world!",
             "keyC": [ "array", "of", "strings" ],
             "keyD": {
                "keyE": "nested",
                "keyF": false,
                "keyG": 123.45,
                "keyH": null
             }
          }
       }
    }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, RunMultipleNativeActions) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](auto value, auto callback) {
        EXPECT_EQ(*value, *UniqueValueFromJson(R"(
          {"type":"string",
           "value":"test1"})"));
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      })
      .WillOnce([&](auto value, auto callback) {
        EXPECT_EQ(*value, *UniqueValueFromJson(R"(
          {"type":"string",
           "value":"test2"})"));
        std::move(callback).Run(ClientStatus(OTHER_ACTION_STATUS), nullptr);
      });

  // Note: the overall flow should report ACTION_APPLIED since the flow
  // completed successfully, but the return value should hold
  // OTHER_ACTION_STATUS, i.e., 3.
  std::unique_ptr<base::Value> result;
  EXPECT_THAT(RunTest(R"(
                        let [status, value] = await runNativeAction('test1');
                        if (status == 2) { // ACTION_APPLIED
                          [status, value] = await runNativeAction('test2');
                        }
                        return status;
                      )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "description": "3",
          "type": "number",
          "value": 3
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, ReturnInteger) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return 12345;", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "description": "12345",
          "type": "number",
          "value": 12345
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, ReturnString) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return 'Hello world!';", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "type": "string",
          "value": "Hello world!"
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, ReturnDictionary) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(
      R"(
          return {
            "keyA":12345,
            "keyB":"Hello world!",
            "keyC": ["array", "of", "strings"],
            "keyD": {
              "keyE": "nested",
              "keyF": true,
              "keyG": 123.45,
              "keyH": null
            }
          };
        )",
      result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "type": "object",
          "value": {
            "keyA": 12345,
            "keyB": "Hello world!",
            "keyC": ["array", "of", "strings"],
            "keyD": {
                "keyE": "nested",
                "keyF": true,
                "keyG": 123.45,
                "keyH": null
            }
          }
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, ReturnNothing) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "type": "undefined"
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, ReturnNull) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return null;", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "subtype": "null",
          "type": "object",
          "value": null
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, ExceptionReporting) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("throw new Error('Hello world!');", result);
  EXPECT_EQ(status.proto_status(), UNEXPECTED_JS_ERROR);
  ASSERT_NE(result, nullptr);

  absl::optional<base::Value> exceptionDetails =
      result->ExtractKey("exceptionDetails");
  ASSERT_NE(exceptionDetails, absl::nullopt);

  EXPECT_THAT(exceptionDetails->ExtractKey("text")->GetIfString(),
              Pointee(Eq("Uncaught (in promise) Error: Hello world!")));

  // We can't currently check the contents of the reported stack frames since
  // they depend on the internal wrapper. For now, we simply test that this is
  // not empty. For reference, at the time of writing, this was the full output:
  // "exceptionDetails": {
  //    "columnNumber": 0,
  //    "exception": {
  //       "className": "Error",
  //       "description": "Error: Hello world!
  //                          at <anonymous>:13:11
  //                          at <anonymous>:13:41",
  //       "objectId": "1450023673453216843.4.1",
  //       "subtype": "error",
  //       "type": "object"
  //    },
  //    "exceptionId": 2,
  //    "lineNumber": 0,
  //    "text": "Uncaught (in promise) Error: Hello world!"
  // }
  EXPECT_THAT(exceptionDetails->ExtractKey("exception"), Ne(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, RunMultipleConsecutiveFlows) {
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<base::Value> result;
    ClientStatus status =
        RunTest(base::StrCat({"return ", base::NumberToString(i)}), result);
    EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
    EXPECT_EQ(result->ExtractKey("result")->ExtractKey("value")->GetIfInt(), i);
  }
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest,
                       UnserializableRunNativeActionArgument) {
  std::unique_ptr<base::Value> result;
  EXPECT_CALL(mock_delegate_, RunNativeAction).Times(0);
  ClientStatus status = RunTest(
      R"(
        function foo(){}
        // foo cannot be serialized as a JSON object, so this should fail.
        let [status, result] = await runNativeAction(foo);
        return status;
      )",
      result);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(status.proto_status(), UNEXPECTED_JS_ERROR);
  EXPECT_TRUE(status.details().has_unexpected_error_info());
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, StartWhileAlreadyRunningFails) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce(WithArg<1>([&](auto callback) {
        // Starting a second flow while the first one is running should fail.
        EXPECT_EQ(RunTest(std::string()).proto_status(), INVALID_ACTION);

        // The first flow should be able to finish successfully.
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));

  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(
      R"(
      let [status, result] = await runNativeAction('');
      return status;
      )",
      result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "description": "2",
          "type": "number",
          "value": 2
        }
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorTest, EnvironmentIsPreservedBetweenRuns) {
  EXPECT_EQ(RunTest("globalFlowState.i = 5;").proto_status(), ACTION_APPLIED);

  std::unique_ptr<base::Value> result;
  EXPECT_EQ(RunTest("return globalFlowState.i;", result).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "result": {
          "description": "5",
          "type": "number",
          "value": 5
        }
      }
    )"));
}

}  // namespace
}  // namespace autofill_assistant
