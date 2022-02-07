// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_executor.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/js_flow_executor_impl.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "content/public/test/browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

class JsFlowExecutorImplTest : public autofill_assistant::BaseBrowserTest {
 public:
  JsFlowExecutorImplTest() {}

  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    flow_executor_ = std::make_unique<JsFlowExecutorImpl>(
        shell()->web_contents(), &mock_delegate_);
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
        js_flow, base::BindOnce(&JsFlowExecutorImplTest::OnFlowFinished,
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
  NiceMock<MockJsFlowExecutorImplDelegate> mock_delegate_;
  std::unique_ptr<JsFlowExecutorImpl> flow_executor_;
};

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, SmokeTest) {
  EXPECT_THAT(RunTest(std::string()),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, InvalidJs) {
  EXPECT_THAT(RunTest("Not valid Javascript"),
              Property(&ClientStatus::proto_status, UNEXPECTED_JS_ERROR));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunNativeActionWithReturnValue) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunNativeActionAsBase64String) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(12, action_id);
        EXPECT_EQ("test", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      });

  std::unique_ptr<base::Value> result;
  EXPECT_THAT(RunTest(R"(
      let [status, value] = await runNativeAction(12, "dGVzdA==" /*test*/);
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
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
  EXPECT_THAT(RunTest(R"(
      let [status, value] = await runNativeAction(
          11, ["aa.msg", "my message"]);
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunMultipleNativeActions) {
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
  EXPECT_THAT(RunTest(R"(
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnInteger) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnString) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnDictionary) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnNothing) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnNull) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ExceptionReporting) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunMultipleConsecutiveFlows) {
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<base::Value> result;
    ClientStatus status =
        RunTest(base::StrCat({"return ", base::NumberToString(i)}), result);
    EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
    EXPECT_EQ(result->ExtractKey("result")->ExtractKey("value")->GetIfInt(), i);
  }
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, StartWhileAlreadyRunningFails) {
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

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
                       EnvironmentIsPreservedBetweenRuns) {
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
