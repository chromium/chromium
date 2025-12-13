// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/call_method.h"

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/dbus/utils/types.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace dbus_utils {
namespace {

constexpr const char kTestServiceName[] = "org.chromium.TestService";
constexpr const char kTestObjectPath[] = "/org/chromium/TestObject";
constexpr const char kTestInterface[] = "org.chromium.TestInterface";
constexpr const char kTestMethodSuccess[] = "MethodSuccess";
constexpr const char kTestMethodSuccessMultiReturn[] =
    "MethodSuccessMultiReturn";
constexpr const char kTestMethodFail[] = "MethodFail";
constexpr const char kTestMethodInvalidResponse[] = "MethodInvalidResponse";
constexpr const char kTestMethodExtraData[] = "MethodExtraData";

template <typename Ret, typename... Args>
base::OnceCallback<Ret(Args...)> RepeatingCallbackToOnceCallback(
    base::RepeatingCallback<Ret(Args...)> callback) {
  return callback;
}

template <typename Lambda>
auto BindLambda(Lambda&& lambda) {
  return RepeatingCallbackToOnceCallback(
      base::BindLambdaForTesting(std::forward<Lambda>(lambda)));
}

// Custom matcher for verifying MethodCall details.
MATCHER_P(IsMethodCall, method, "") {
  return arg->GetMember() == method;
}

class CallMethodTest : public testing::Test {
 public:
  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(StrEq(kTestServiceName),
                                           dbus::ObjectPath(kTestObjectPath)))
        .WillRepeatedly(Return(mock_proxy_.get()));
  }

  void TearDown() override {
    mock_proxy_.reset();
    mock_bus_.reset();
  }

 protected:
  // Helper to set up the expectation for CallMethodWithErrorResponse.
  // It saves the callback provided by CallMethod.
  void ExpectCallMethodWithErrorResponse(const std::string& expected_method) {
    EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(
                                  IsMethodCall(expected_method),
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          response_or_error_callback_ = std::move(callback);
        });
  }

  // Runs the captured callback with a success response containing the provided
  // writer_callback to populate the response.
  template <typename WriterCallback>
  void SimulateSuccessResponse(WriterCallback writer_callback) {
    response_ = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response_.get());
    writer_callback(&writer);

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_or_error_callback_),
                                  response_.get(), nullptr));
  }

  // Runs the captured callback with an error response.
  void SimulateErrorResponse(const std::string& interface,
                             const std::string& method) {
    dbus::MethodCall method_call(interface, method);
    method_call.SetSerial(1234);
    error_response_ = dbus::ErrorResponse::FromMethodCall(
        &method_call, "org.freedesktop.DBus.Error.Failed", "Test Error");

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_or_error_callback_),
                                  nullptr, error_response_.get()));
  }

  // Runs the captured callback with no response (timeout or other failure).
  void SimulateNoResponse() {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_or_error_callback_),
                                  nullptr, nullptr));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  dbus::ObjectProxy::ResponseOrErrorCallback response_or_error_callback_;
  std::unique_ptr<dbus::Response> response_;
  std::unique_ptr<dbus::ErrorResponse> error_response_;
};

TEST_F(CallMethodTest, SuccessNoReturnValue) {
  ExpectCallMethodWithErrorResponse(kTestMethodSuccess);

  bool success = false;
  base::RunLoop run_loop;

  CallMethod<"", "">(mock_proxy_.get(), kTestInterface, kTestMethodSuccess,
                     BindLambda([&](CallMethodResult<> result) {
                       ASSERT_TRUE(result.has_value());
                       success = true;
                       run_loop.Quit();
                     }));

  SimulateSuccessResponse([](dbus::MessageWriter* writer) {});
  run_loop.Run();

  EXPECT_TRUE(success);
}

TEST_F(CallMethodTest, SuccessOneReturnValue) {
  const std::string kExpectedString = "Success!";
  ExpectCallMethodWithErrorResponse(kTestMethodSuccess);

  std::optional<std::string> returned_string;
  base::RunLoop run_loop;

  CallMethod<"", "s">(mock_proxy_.get(), kTestInterface, kTestMethodSuccess,
                      BindLambda([&](CallMethodResult<std::string> result) {
                        ASSERT_TRUE(result.has_value());
                        returned_string = std::get<0>(result.value());
                        run_loop.Quit();
                      }));

  SimulateSuccessResponse([&](dbus::MessageWriter* writer) {
    writer->AppendString(kExpectedString);
  });
  run_loop.Run();

  EXPECT_EQ(returned_string, kExpectedString);
}

TEST_F(CallMethodTest, SuccessMultipleReturnValues) {
  const std::string kExpectedString = "Multiple";
  const uint32_t kExpectedUint = 42;
  const bool kExpectedBool = true;

  ExpectCallMethodWithErrorResponse(kTestMethodSuccessMultiReturn);

  std::optional<std::string> returned_string;
  std::optional<uint32_t> returned_uint;
  std::optional<bool> returned_bool;
  base::RunLoop run_loop;

  CallMethod<"", "sub">(
      mock_proxy_.get(), kTestInterface, kTestMethodSuccessMultiReturn,
      BindLambda([&](CallMethodResult<std::string, uint32_t, bool> result) {
        ASSERT_TRUE(result.has_value());
        std::tie(returned_string, returned_uint, returned_bool) =
            result.value();
        run_loop.Quit();
      }));

  SimulateSuccessResponse([&](dbus::MessageWriter* writer) {
    writer->AppendString(kExpectedString);
    writer->AppendUint32(kExpectedUint);
    writer->AppendBool(kExpectedBool);
  });
  run_loop.Run();

  EXPECT_EQ(returned_string, kExpectedString);
  EXPECT_EQ(returned_uint, kExpectedUint);
  EXPECT_EQ(returned_bool, kExpectedBool);
}

TEST_F(CallMethodTest, ErrorResponse) {
  ExpectCallMethodWithErrorResponse(kTestMethodFail);

  std::optional<CallMethodErrorStatus> result_error;
  base::RunLoop run_loop;

  CallMethod<"", "">(mock_proxy_.get(), kTestInterface, kTestMethodFail,
                     BindLambda([&](CallMethodResult<> result) {
                       ASSERT_FALSE(result.has_value());
                       result_error = result.error().status;
                       run_loop.Quit();
                     }));

  SimulateErrorResponse(kTestInterface, kTestMethodFail);
  run_loop.Run();

  EXPECT_EQ(result_error, CallMethodErrorStatus::kErrorResponse);
}

TEST_F(CallMethodTest, NoResponse) {
  ExpectCallMethodWithErrorResponse(kTestMethodFail);

  std::optional<CallMethodErrorStatus> result_error;
  base::RunLoop run_loop;

  CallMethod<"", "">(mock_proxy_.get(), kTestInterface, kTestMethodFail,
                     BindLambda([&](CallMethodResult<> result) {
                       ASSERT_FALSE(result.has_value());
                       result_error = result.error().status;
                       run_loop.Quit();
                     }));

  SimulateNoResponse();
  run_loop.Run();

  EXPECT_EQ(result_error, CallMethodErrorStatus::kNoResponse);
}

TEST_F(CallMethodTest, InvalidResponseFormat_WrongType) {
  ExpectCallMethodWithErrorResponse(kTestMethodInvalidResponse);

  std::optional<CallMethodErrorStatus> result_error;
  base::RunLoop run_loop;

  CallMethod<"", "s">(mock_proxy_.get(), kTestInterface,
                      kTestMethodInvalidResponse,
                      BindLambda([&](CallMethodResult<std::string> result) {
                        ASSERT_FALSE(result.has_value());
                        result_error = result.error().status;
                        run_loop.Quit();
                      }));

  // Simulate a response containing a boolean instead of a string.
  SimulateSuccessResponse(
      [&](dbus::MessageWriter* writer) { writer->AppendBool(true); });
  run_loop.Run();

  EXPECT_EQ(result_error, CallMethodErrorStatus::kInvalidResponseFormat);
}

TEST_F(CallMethodTest, InvalidResponseFormat_MissingData) {
  ExpectCallMethodWithErrorResponse(kTestMethodInvalidResponse);

  std::optional<CallMethodErrorStatus> result_error;
  base::RunLoop run_loop;

  CallMethod<"", "s">(mock_proxy_.get(), kTestInterface,
                      kTestMethodInvalidResponse,
                      BindLambda([&](CallMethodResult<std::string> result) {
                        ASSERT_FALSE(result.has_value());
                        result_error = result.error().status;
                        run_loop.Quit();
                      }));

  // Simulate an empty response (no data to read the string from).
  SimulateSuccessResponse([](dbus::MessageWriter* writer) {});
  run_loop.Run();

  EXPECT_EQ(result_error, CallMethodErrorStatus::kInvalidResponseFormat);
}

TEST_F(CallMethodTest, ExtraDataInResponse) {
  const std::string kExpectedString = "Expected Data";
  const int kExtraInt = 123;

  ExpectCallMethodWithErrorResponse(kTestMethodExtraData);

  std::optional<CallMethodErrorStatus> result_error;
  base::RunLoop run_loop;

  CallMethod<"", "s">(mock_proxy_.get(), kTestInterface, kTestMethodExtraData,
                      BindLambda([&](CallMethodResultSig<"s"> result) {
                        ASSERT_FALSE(result.has_value());
                        result_error = result.error().status;
                        run_loop.Quit();
                      }));

  // Simulate a response containing the expected string AND an extra integer.
  SimulateSuccessResponse([&](dbus::MessageWriter* writer) {
    writer->AppendString(kExpectedString);
    writer->AppendInt32(kExtraInt);
  });
  run_loop.Run();

  EXPECT_EQ(result_error, CallMethodErrorStatus::kExtraDataInResponse);
}

TEST_F(CallMethodTest, ArgumentsPassedCorrectly) {
  const std::string kArgString = "ArgumentValue";
  const uint32_t kArgUint = 99;

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(
                                IsMethodCall(kTestMethodSuccess),
                                dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(method_call);
        std::string received_string;
        uint32_t received_uint;
        ASSERT_TRUE(reader.PopString(&received_string));
        ASSERT_TRUE(reader.PopUint32(&received_uint));
        EXPECT_EQ(received_string, kArgString);
        EXPECT_EQ(received_uint, kArgUint);
        EXPECT_FALSE(reader.HasMoreData());

        response_or_error_callback_ = std::move(callback);
      });

  bool success = false;
  base::RunLoop run_loop;

  CallMethod<"su", "">(mock_proxy_.get(), kTestInterface, kTestMethodSuccess,
                       BindLambda([&](CallMethodResult<> result) {
                         ASSERT_TRUE(result.has_value());
                         success = true;
                         run_loop.Quit();
                       }),
                       kArgString, kArgUint);

  SimulateSuccessResponse([](dbus::MessageWriter* writer) {});
  run_loop.Run();

  EXPECT_TRUE(success);
}

TEST_F(CallMethodTest, ArgsConvertsTypes) {
  ExpectCallMethodWithErrorResponse(kTestMethodSuccess);

  bool success = false;
  base::RunLoop run_loop;

  // Intentionally pass "42" as an int literal, even though the signature
  // specifies a uint32_t.
  CallMethod<"u", "">(mock_proxy_.get(), kTestInterface, kTestMethodSuccess,
                      BindLambda([&](CallMethodResult<> result) {
                        ASSERT_TRUE(result.has_value());
                        success = true;
                        run_loop.Quit();
                      }),
                      42);

  SimulateSuccessResponse([](dbus::MessageWriter* writer) {});
  run_loop.Run();

  EXPECT_TRUE(success);
}

TEST_F(CallMethodTest, AllowBaseDoNothingComplies) {
  ExpectCallMethodWithErrorResponse(kTestMethodSuccess);

  CallMethod<"", "">(mock_proxy_.get(), kTestInterface, kTestMethodSuccess,
                     base::DoNothing());

  SimulateSuccessResponse([](dbus::MessageWriter* writer) {});
}

TEST_F(CallMethodTest, AllowBaseDoNothingCompliesWithArgs) {
  ExpectCallMethodWithErrorResponse(kTestMethodSuccess);

  CallMethod<"is", "si">(mock_proxy_.get(), kTestInterface, kTestMethodSuccess,
                         base::DoNothing(), 123, "test");

  SimulateSuccessResponse([](dbus::MessageWriter* writer) {});
}

}  // namespace
}  // namespace dbus_utils
