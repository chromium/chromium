// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_executor.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_context.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/js_flow_executor_impl.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/mock_script_executor_delegate.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::WithArg;

class MockJsFlowExecutorImplDelegate : public JsFlowExecutorImpl::Delegate {
 public:
  MockJsFlowExecutorImplDelegate() = default;
  ~MockJsFlowExecutorImplDelegate() override = default;

  MOCK_METHOD(
      void,
      RunNativeAction,
      (int,
       const std::string&,
       base::OnceCallback<void(const ClientStatus& result_status,
                               std::unique_ptr<base::Value> result_value)>
           callback),
      (override));
};

class JsFlowExecutorImplBrowserTest : public BaseBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    js_flow_devtools_wrapper_ =
        std::make_unique<JsFlowDevtoolsWrapper>(shell()->web_contents());
    flow_executor_ = std::make_unique<JsFlowExecutorImpl>(
        &mock_delegate_, js_flow_devtools_wrapper_.get());
  }

  // Overload, ignore result value, just return the client status.
  ClientStatus RunTest(const std::string& js_flow,
                       std::pair<std::string, std::string> startup_param =
                           std::make_pair(std::string(), std::string())) {
    std::unique_ptr<base::Value> ignored_result;
    return RunTest(js_flow, ignored_result, startup_param);
  }

  ClientStatus RunTest(const std::string& js_flow,
                       std::unique_ptr<base::Value>& result_value,
                       std::pair<std::string, std::string> startup_param =
                           std::make_pair(std::string(), std::string())) {
    ClientStatus status;
    base::RunLoop run_loop;
    absl::optional<std::pair<std::string, std::string>> opt_startup_param;
    if (!startup_param.first.empty()) {
      opt_startup_param = startup_param;
    }

    flow_executor_->Start(
        js_flow, opt_startup_param,
        base::BindOnce(&JsFlowExecutorImplBrowserTest::OnFlowFinished,
                       base::Unretained(this), run_loop.QuitClosure(), &status,
                       std::ref(result_value)));
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
  NiceMock<MockJsFlowExecutorImplDelegate> mock_delegate_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<JsFlowExecutorImpl> flow_executor_;
  std::unique_ptr<JsFlowDevtoolsWrapper> js_flow_devtools_wrapper_;
};

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, SmokeTest) {
  EXPECT_THAT(RunTest(std::string()),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  histogram_tester_.ExpectBucketCount(
      "Android.AutofillAssistant.JsFlowStartedEvent",
      Metrics::JsFlowStartedEvent::EXECUTOR_STARTED, 1);
  histogram_tester_.ExpectBucketCount(
      "Android.AutofillAssistant.JsFlowStartedEvent",
      Metrics::JsFlowStartedEvent::SCRIPT_STARTED, 1);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, InvalidJs) {
  EXPECT_THAT(RunTest("Not valid Javascript"),
              Property(&ClientStatus::proto_status, UNEXPECTED_JS_ERROR));

  histogram_tester_.ExpectBucketCount(
      "Android.AutofillAssistant.JsFlowStartedEvent",
      Metrics::JsFlowStartedEvent::EXECUTOR_STARTED, 1);
  histogram_tester_.ExpectBucketCount(
      "Android.AutofillAssistant.JsFlowStartedEvent",
      Metrics::JsFlowStartedEvent::SCRIPT_STARTED, 1);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       RunNativeActionWithReturnValue) {
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
            },
            "keyI": [1,2,3,4]
          }
        )")));

  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(12, action_id);
        EXPECT_EQ("test", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED),
                                std::move(native_return_value));
      });

  std::unique_ptr<base::Value> js_return_value;
  EXPECT_THAT(RunTest(R"(
                        let [status, value] = await runNativeAction(
                            12, "dGVzdA==" /*test*/);
                        if (status != 2) { // ACTION_APPLIED
                          return status;
                        }

                        // Remove non-allowed types from return value.
                        delete value.keyB;
                        delete value.keyC;
                        delete value.keyD.keyE;

                        // Make some changes just to check that this propagates.
                        value.keyA += 3;
                        value.keyD.keyF = false;
                        value.keyI.push(5);
                        return value;
                      )",
                      js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*js_return_value, *base::JSONReader::Read(R"(
    {
      "keyA": 12348,
      "keyD": {
        "keyF": false,
        "keyG": 123.45,
        "keyH": null
      },
      "keyI": [1,2,3,4,5]
    }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       RunNativeActionAsBase64String) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(12, action_id);
        EXPECT_EQ("test", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      });

  std::unique_ptr<base::Value> result;
  ASSERT_THAT(RunTest(R"(
      let [status, value] = await runNativeAction(12, "dGVzdA==" /*test*/);
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  // ACTION_APPLIED == 2.
  EXPECT_EQ(*result, base::Value(2));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       RunNativeActionAsSerializedProto) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(ActionProto::kTell, action_id);
        TellProto tell;
        EXPECT_TRUE(tell.ParseFromString(action));
        EXPECT_EQ(tell.message(), "my message");
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      });

  std::unique_ptr<base::Value> result;
  ASSERT_THAT(RunTest(R"(
      let [status, value] = await runNativeAction(
          11, ["aa.msg", "my message"]);
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*result, base::Value(2));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       RunMultipleNativeActions) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(1, action_id);
        EXPECT_EQ("test1", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      })
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(2, action_id);
        EXPECT_EQ("test2", action);
        std::move(callback).Run(ClientStatus(OTHER_ACTION_STATUS), nullptr);
      });

  // Note: the overall flow should report ACTION_APPLIED since the flow
  // completed successfully, but the return value should hold
  // OTHER_ACTION_STATUS, i.e., 3.
  std::unique_ptr<base::Value> result;
  ASSERT_THAT(RunTest(R"(
                        let [status, value] = await runNativeAction(
                            1, "dGVzdDE=" /*test1*/);
                        if (status == 2) { // ACTION_APPLIED
                          [status, value] = await runNativeAction(
                            2, "dGVzdDI=" /*test2*/);
                        }
                        return status;
                      )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  // OTHER_ACTION_STATUS == 3
  EXPECT_EQ(*result, base::Value(3));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, ReturnInteger) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return 12345;", result);
  ASSERT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, base::Value(12345));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, ReturningStringFails) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return 'Strings are not allowed!';", result);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
  EXPECT_THAT(result, Eq(nullptr));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, ReturnDictionary) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(
      R"(
          return {
            "keyA":12345,
            "keyB": {
              "keyC": true,
              "keyD": 123.45,
              "keyE": null
            },
            "keyF": [false, false, true],
            "keyG": "string"
          };
        )",
      result);
  ASSERT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "keyA":12345,
          "keyB": {
            "keyC": true,
            "keyD": 123.45,
            "keyE": null
          },
        "keyF": [false, false, true],
        "keyG": "string"
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, ReturnNothing) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_THAT(result, Eq(nullptr));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       ReturnNonJsonObjectFails) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(R"(
    function test() {
      console.log('something');
    }
    return test;
  )",
                                result);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
  EXPECT_THAT(result, Eq(nullptr));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, ReturnNull) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return null;", result);
  ASSERT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read("null"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, ExceptionReporting) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("notdefined;", result);
  EXPECT_EQ(status.proto_status(), UNEXPECTED_JS_ERROR);
  ASSERT_THAT(result, Eq(nullptr));

  // NOTE: Do not change the values here. The above script should output exactly
  // one stack frame with 0:0.
  EXPECT_THAT(
      status.details().unexpected_error_info().js_exception_line_numbers(),
      ElementsAre(0));
  EXPECT_THAT(
      status.details().unexpected_error_info().js_exception_column_numbers(),
      ElementsAre(0));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       RunMultipleConsecutiveFlows) {
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<base::Value> result;
    ClientStatus status =
        RunTest(base::StrCat({"return ", base::NumberToString(i)}), result);
    ASSERT_EQ(status.proto_status(), ACTION_APPLIED);
    EXPECT_EQ(*result, base::Value(i));
  }
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       LineOffsetIsSetCorrectly) {
  const std::string js_flow =
      // We override the prepareStackTrace function to gain access to the
      // CallSite objects (see v8.dev/docs/stack-trace-api).
      // NOTE: There is no newline below.
      "Error.prepareStackTrace = (_, structuredStack) => structuredStack;"
      "const topStackFrame = new Error().stack[0];"
      "return topStackFrame.getLineNumber() - LINE_OFFSET;";

  std::unique_ptr<base::Value> js_return_value;
  ASSERT_THAT(RunTest(js_flow, js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(js_return_value, NotNull());
  // line number is 1-based in getLineNumber so this is the first line
  EXPECT_THAT(js_return_value->GetIfInt(), Optional(1));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, DebugModeIsSetCorrectly) {
  const std::string js_flow = "return DEBUG_MODE;";

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kAutofillAssistantDebugMode);
  command_line->AppendSwitchASCII(switches::kAutofillAssistantDebugMode,
                                  "true");

  std::unique_ptr<base::Value> js_return_value;
  ASSERT_THAT(RunTest(js_flow, js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(js_return_value, NotNull());
  EXPECT_EQ(js_return_value->GetIfBool(), true);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       NonDebugModeIsSetCorrectly) {
  const std::string js_flow = "return DEBUG_MODE;";

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kAutofillAssistantDebugMode);
  command_line->AppendSwitchASCII(switches::kAutofillAssistantDebugMode,
                                  "false");

  std::unique_ptr<base::Value> js_return_value;
  ASSERT_THAT(RunTest(js_flow, js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(js_return_value, NotNull());
  EXPECT_EQ(js_return_value->GetIfBool(), false);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest, NonDebugModeIsDefault) {
  const std::string js_flow = "return DEBUG_MODE;";

  std::unique_ptr<base::Value> js_return_value;
  ASSERT_THAT(RunTest(js_flow, js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(js_return_value, NotNull());
  EXPECT_EQ(js_return_value->GetIfBool(), false);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       PlatformTypeIsPassedCorrectly) {
  const std::string js_flow = "return PLATFORM_TYPE;";

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kAutofillAssistantDebugMode);
  command_line->AppendSwitchASCII(switches::kAutofillAssistantDebugMode,
                                  "false");

  std::unique_ptr<base::Value> js_return_value;
  ASSERT_THAT(RunTest(js_flow, js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(js_return_value, NotNull());
  const absl::optional<int> platform_type_int = js_return_value->GetIfInt();
  ASSERT_TRUE(platform_type_int.has_value());
  EXPECT_EQ(static_cast<ClientContextProto::PlatformType>(*platform_type_int),
            ClientContext::GetPlatformType());
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       VersionNumberIsSetCorrectly) {
  const std::string js_flow = "return {versionNumber: CHROME_VERSION_NUMBER};";

  std::unique_ptr<base::Value> js_return_value;
  ASSERT_THAT(RunTest(js_flow, js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(js_return_value, NotNull());
  EXPECT_TRUE(js_return_value->is_dict());
  EXPECT_THAT(js_return_value->GetIfDict()->FindString("versionNumber"),
              Pointee(version_info::GetVersionNumber()));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       UnserializableRunNativeActionString) {
  std::unique_ptr<base::Value> result;
  EXPECT_CALL(mock_delegate_, RunNativeAction).Times(0);
  ClientStatus status = RunTest(
      R"(
        // {} is not a string or an array, so this should fail.
        let [status, result] = await runNativeAction(1, {});
        return status;
      )",
      result);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       UnserializableRunNativeActionId) {
  std::unique_ptr<base::Value> result;
  EXPECT_CALL(mock_delegate_, RunNativeAction).Times(0);
  ClientStatus status = RunTest(
      R"(
        // {} is not a number, so this should fail.
        let [status, result] = await runNativeAction({}, "");
        return status;
      )",
      result);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       StartWhileAlreadyRunningFails) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce(WithArg<2>([&](auto callback) {
        // Starting a second flow while the first one is running should fail.
        EXPECT_EQ(RunTest(std::string()).proto_status(), INVALID_ACTION);

        // The first flow should be able to finish successfully.
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));

  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(
      R"(
      let [status, result] = await runNativeAction(1, "dGVzdA==" /*test*/);
      return status;
      )",
      result);
  ASSERT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, base::Value(2));

  histogram_tester_.ExpectBucketCount(
      "Android.AutofillAssistant.JsFlowStartedEvent",
      Metrics::JsFlowStartedEvent::EXECUTOR_STARTED, 2);
  histogram_tester_.ExpectBucketCount(
      "Android.AutofillAssistant.JsFlowStartedEvent",
      Metrics::JsFlowStartedEvent::FAILED_ALREADY_RUNNING, 1);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       EnvironmentIsPreservedBetweenRuns) {
  EXPECT_EQ(RunTest("globalFlowState.i = 5;").proto_status(), ACTION_APPLIED);

  std::unique_ptr<base::Value> result;
  ASSERT_EQ(RunTest("return globalFlowState.i;", result).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(*result, base::Value(5));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       JsFlowLibraryIsAvailable) {
  js_flow_devtools_wrapper_->SetJsFlowLibrary("const status = 2;");

  std::unique_ptr<base::Value> result;
  ASSERT_THAT(RunTest(R"(
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*result, base::Value(2));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplBrowserTest,
                       StartupParamIsSetCorrectly) {
  const std::string js_flow = "return {startupParam: MY_STARTUP_PARAM};";

  std::unique_ptr<base::Value> result;
  EXPECT_THAT(RunTest(js_flow, result, /* startup_param = */
                      std::make_pair(std::string("MY_STARTUP_PARAM"),
                                     std::string("hello world"))),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));

  ASSERT_THAT(result, NotNull());
  EXPECT_TRUE(result->is_dict());
  EXPECT_THAT(result->GetIfDict()->FindString("startupParam"),
              Pointee(std::string("hello world")));
}

}  // namespace
}  // namespace autofill_assistant
