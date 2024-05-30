// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/regmon/regmon_client.h"

#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/regmon/regmon_service.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/regmon/dbus-constants.h"

using ::logging::LOGGING_ERROR;

using ::testing::_;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

class RegmonClientTest : public testing::Test {
 public:
  RegmonClientTest() = default;

  RegmonClientTest(const RegmonClientTest&) = delete;
  RegmonClientTest& operator=(const RegmonClientTest&) = delete;

  ~RegmonClientTest() override = default;

  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());

    // Setup MockObjectProxy. This is used to mock out the D-Bus calls made via
    // CallMethod(). Each test creates its own D-Bus response and invokes the
    // passed in ResponseCallback with the response being tested.
    proxy_ =
        new dbus::MockObjectProxy(bus_.get(), regmon::kRegmonServiceName,
                                  dbus::ObjectPath(regmon::kRegmonServicePath));
    EXPECT_CALL(*bus_,
                GetObjectProxy(regmon::kRegmonServiceName,
                               dbus::ObjectPath(regmon::kRegmonServicePath)))
        .WillRepeatedly(Return(proxy_.get()));

    RegmonClient::Initialize(bus_.get());

    mock_log_.StartCapturingLogs();
  }

  void TearDown() override {
    RegmonClient::Shutdown();
    mock_log_.StopCapturingLogs();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls to regmond.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Mock log for capturing and verifying error output.
  base::test::MockLog mock_log_;
};

TEST_F(RegmonClientTest, RecordPolicyViolation_SuccessTest) {
  const int32_t kTestHashCode = 123;

  EXPECT_CALL(*proxy_, DoCallMethod)
      .WillOnce(Invoke([&](dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        // First verify that the request proto was included in the dbus call.
        dbus::MessageReader reader(method_call);
        regmon::RecordPolicyViolationRequest request;
        if (!reader.PopArrayOfBytesAsProto(&request)) {
          FAIL() << "Failed to parse RecordPolicyViolationRequest";
        }
        EXPECT_EQ(request.violation().annotation_hash(), kTestHashCode);

        // Run callback with successful dbus response (no errors).
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendProtoAsArrayOfBytes(
            regmon::RecordPolicyViolationResponse());
        std::move(*callback).Run(response.get());
      }));

  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, _, _, _, _)).Times(0);

  regmon::RecordPolicyViolationRequest request;
  request.mutable_violation()->set_annotation_hash(kTestHashCode);
  RegmonClient::Get()->RecordPolicyViolation(request);
}

TEST_F(RegmonClientTest, RecordPolicyViolation_ErrorTest) {
  const std::string kRegmonErrorMessage = "Mock error from regmon";

  EXPECT_CALL(*proxy_, DoCallMethod)
      .WillOnce(Invoke([&](dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        // Run callback with a dbus response containing an error from regmond.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());

        regmon::RecordPolicyViolationResponse regmon_response;
        auto* status = regmon_response.mutable_status();
        status->set_error_message(kRegmonErrorMessage);

        writer.AppendProtoAsArrayOfBytes(regmon_response);
        std::move(*callback).Run(response.get());
      }));

  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, _, _, _, _))
      .Times(testing::AnyNumber());  // ignore uninteresting log calls
  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, EndsWith("regmon_client.cc"), _, _,
                             HasSubstr(kRegmonErrorMessage)))
      .Times(1);

  RegmonClient::Get()->RecordPolicyViolation(
      regmon::RecordPolicyViolationRequest());
}

TEST_F(RegmonClientTest, RecordPolicyViolation_NoResponseTest) {
  EXPECT_CALL(*proxy_, DoCallMethod)
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        // Run callback with null dbus response. This should cause a no-response
        // error in the callback.
        std::move(*callback).Run(nullptr);
      }));

  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, _, _, _, _))
      .Times(testing::AnyNumber());  // ignore uninteresting log calls
  EXPECT_CALL(mock_log_,
              Log(LOGGING_ERROR, EndsWith("regmon_client.cc"), _, _,
                  HasSubstr("No response message received from regmon")))
      .Times(1);

  RegmonClient::Get()->RecordPolicyViolation(
      regmon::RecordPolicyViolationRequest());
}

TEST_F(RegmonClientTest, RecordPolicyViolation_ResponseParseFailureTest) {
  EXPECT_CALL(*proxy_, DoCallMethod)
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        // Run callback with empty dbus response. This should cause a response
        // parsing error in the callback.
        std::move(*callback).Run(dbus::Response::CreateEmpty().get());
      }));

  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, _, _, _, _))
      .Times(testing::AnyNumber());  // ignore uninteresting log calls
  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, EndsWith("regmon_client.cc"), _, _,
                             HasSubstr("Failed to parse response message")))
      .Times(1);

  RegmonClient::Get()->RecordPolicyViolation(
      regmon::RecordPolicyViolationRequest());
}

}  // namespace chromeos
