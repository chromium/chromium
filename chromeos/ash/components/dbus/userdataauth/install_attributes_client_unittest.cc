// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/install_attributes_client.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace ash {

namespace {

// Runs |callback| with |response|. Needed due to ResponseCallback expecting a
// bare pointer rather than an std::unique_ptr.
void RunResponseCallback(dbus::ObjectProxy::ResponseCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get());
}

bool ProtobufEquals(const google::protobuf::MessageLite& a,
                    const google::protobuf::MessageLite& b) {
  std::string a_serialized, b_serialized;
  a.SerializeToString(&a_serialized);
  b.SerializeToString(&b_serialized);
  return a_serialized == b_serialized;
}

// FakeTaskRunner will run all tasks posted to it immediately in the PostTask()
// call. This class is a helper to ensure that BlockingMethodCaller would work
// correctly in the unit test. Note that Mock is not used because
// SingleThreadTaskRunner is refcounted and it doesn't play well with Mock.
class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  // Yes, this task runner runs everything in sequence.
  bool RunsTasksInCurrentSequence() const override { return true; }

  // Run all tasks immediately, no delay is allowed.
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure closure,
                       base::TimeDelta delta) override {
    // Since we are running it now, we can't accept any delay.
    CHECK(delta.is_zero());
    std::move(closure).Run();
    return true;
  }

  // Non nestable task not supported.
  bool PostNonNestableDelayedTask(const base::Location& location,
                                  base::OnceClosure closure,
                                  base::TimeDelta delta) override {
    // Can't run non-nested stuff.
    NOTIMPLEMENTED();
    return false;
  }

 private:
  // For reference counting.
  ~FakeTaskRunner() override {}
};

// Create a callback that would copy the input argument passed to it into |out|.
// This is used mostly to create a callback that would catch the reply from
// dbus.
template <typename T>
base::OnceCallback<void(T)> CreateCopyCallback(T* out) {
  return base::BindOnce([](T* out, T result) { *out = result; }, out);
}

}  // namespace

class InstallAttributesClientTest : public testing::Test {
 public:
  InstallAttributesClientTest() = default;
  ~InstallAttributesClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    dbus::ObjectPath userdataauth_object_path =
        dbus::ObjectPath(::user_data_auth::kUserDataAuthServicePath);
    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), ::user_data_auth::kUserDataAuthServiceName,
        userdataauth_object_path);

    // Makes sure `GetObjectProxy()` is called with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::user_data_auth::kUserDataAuthServiceName,
                               userdataauth_object_path))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_CALL(*proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(this, &InstallAttributesClientTest::OnCallMethod));
    EXPECT_CALL(*proxy_.get(), CallMethodAndBlock(_, _))
        .WillRepeatedly(
            Invoke(this, &InstallAttributesClientTest::OnBlockingCallMethod));

    InstallAttributesClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = InstallAttributesClient::Get();
  }

  void TearDown() override { InstallAttributesClient::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  raw_ptr<InstallAttributesClient, ExperimentalAsh> client_;

  // The expected replies to the respective D-Bus calls.
  ::user_data_auth::InstallAttributesGetReply
      expected_install_attributes_get_reply_;
  ::user_data_auth::InstallAttributesSetReply
      expected_install_attributes_set_reply_;
  ::user_data_auth::InstallAttributesFinalizeReply
      expected_install_attributes_finalize_reply_;
  ::user_data_auth::InstallAttributesGetStatusReply
      expected_install_attributes_get_status_reply_;
  ::user_data_auth::RemoveFirmwareManagementParametersReply
      expected_remove_firmware_management_parameters_reply_;
  ::user_data_auth::SetFirmwareManagementParametersReply
      expected_set_firmware_management_parameters_reply_;
  ::user_data_auth::GetFirmwareManagementParametersReply
      expected_get_firmware_management_parameters_reply_;

  // The expected replies to the respective blocking D-Bus calls.
  ::user_data_auth::InstallAttributesGetReply
      expected_blocking_install_attributes_get_reply_;
  ::user_data_auth::InstallAttributesSetReply
      expected_blocking_install_attributes_set_reply_;
  ::user_data_auth::InstallAttributesFinalizeReply
      expected_blocking_install_attributes_finalize_reply_;
  ::user_data_auth::InstallAttributesGetStatusReply
      expected_blocking_install_attributes_get_status_reply_;

  // When it is set `true`, an invalid array of bytes that cannot be parsed will
  // be the response.
  bool shall_message_parsing_fail_ = false;

 private:
  // Handles calls to |proxy_|'s `CallMethod()`.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* callback) {
    std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    if (shall_message_parsing_fail_) {
      // 0x02 => Field 0, Type String
      // (0xFF)*6 => Varint, the size of the string, it is not terminated and is
      // a very large value so the parsing will fail.
      constexpr uint8_t invalid_protobuf[] = {0x02, 0xFF, 0xFF, 0xFF,
                                              0xFF, 0xFF, 0xFF};
      writer.AppendArrayOfBytes(invalid_protobuf, sizeof(invalid_protobuf));
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesGet) {
      writer.AppendProtoAsArrayOfBytes(expected_install_attributes_get_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesFinalize) {
      writer.AppendProtoAsArrayOfBytes(
          expected_install_attributes_finalize_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesGetStatus) {
      writer.AppendProtoAsArrayOfBytes(
          expected_install_attributes_get_status_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kRemoveFirmwareManagementParameters) {
      writer.AppendProtoAsArrayOfBytes(
          expected_remove_firmware_management_parameters_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kSetFirmwareManagementParameters) {
      writer.AppendProtoAsArrayOfBytes(
          expected_set_firmware_management_parameters_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetFirmwareManagementParameters) {
      writer.AppendProtoAsArrayOfBytes(
          expected_get_firmware_management_parameters_reply_);
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }

  // Handles blocking call to |proxy_|'s `CallMethodAndBlock`.
  base::expected<std::unique_ptr<dbus::Response>, dbus::Error>
  OnBlockingCallMethod(dbus::MethodCall* method_call, int timeout_ms) {
    std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    if (shall_message_parsing_fail_) {
      // 0x02 => Field 0, Type String
      // (0xFF)*6 => Varint, the size of the string, it is not terminated and is
      // a very large value so the parsing will fail.
      constexpr uint8_t invalid_protobuf[] = {0x02, 0xFF, 0xFF, 0xFF,
                                              0xFF, 0xFF, 0xFF};
      writer.AppendArrayOfBytes(invalid_protobuf, sizeof(invalid_protobuf));
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesGet) {
      writer.AppendProtoAsArrayOfBytes(
          expected_blocking_install_attributes_get_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesSet) {
      writer.AppendProtoAsArrayOfBytes(
          expected_blocking_install_attributes_set_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesFinalize) {
      writer.AppendProtoAsArrayOfBytes(
          expected_blocking_install_attributes_finalize_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kInstallAttributesGetStatus) {
      writer.AppendProtoAsArrayOfBytes(
          expected_blocking_install_attributes_get_status_reply_);
    } else {
      LOG(FATAL) << "Unrecognized member: " << method_call->GetMember();
      return nullptr;
    }
    return base::ok(std::move(response));
  }
};

TEST_F(InstallAttributesClientTest, InstallAttributesGet) {
  expected_install_attributes_get_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesGetReply> result_reply;

  client_->InstallAttributesGet(::user_data_auth::InstallAttributesGetRequest(),
                                CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_install_attributes_get_reply_));
}

TEST_F(InstallAttributesClientTest, InstallAttributesGetInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  absl::optional<::user_data_auth::InstallAttributesGetReply> result_reply =
      ::user_data_auth::InstallAttributesGetReply();

  client_->InstallAttributesGet(::user_data_auth::InstallAttributesGetRequest(),
                                CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_reply, absl::nullopt);
}

TEST_F(InstallAttributesClientTest, InstallAttributesFinalize) {
  expected_install_attributes_finalize_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesFinalizeReply> result_reply;

  client_->InstallAttributesFinalize(
      ::user_data_auth::InstallAttributesFinalizeRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_install_attributes_finalize_reply_));
}

TEST_F(InstallAttributesClientTest, InstallAttributesGetStatus) {
  expected_install_attributes_get_status_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesGetStatusReply>
      result_reply;

  client_->InstallAttributesGetStatus(
      ::user_data_auth::InstallAttributesGetStatusRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_install_attributes_get_status_reply_));
}

TEST_F(InstallAttributesClientTest, RemoveFirmwareManagementParameters) {
  expected_remove_firmware_management_parameters_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::RemoveFirmwareManagementParametersReply>
      result_reply;

  client_->RemoveFirmwareManagementParameters(
      ::user_data_auth::RemoveFirmwareManagementParametersRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_remove_firmware_management_parameters_reply_));
}

TEST_F(InstallAttributesClientTest, SetFirmwareManagementParameters) {
  expected_set_firmware_management_parameters_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::SetFirmwareManagementParametersReply>
      result_reply;

  client_->SetFirmwareManagementParameters(
      ::user_data_auth::SetFirmwareManagementParametersRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_set_firmware_management_parameters_reply_));
}

TEST_F(InstallAttributesClientTest, GetFirmwareManagementParameters) {
  expected_set_firmware_management_parameters_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::GetFirmwareManagementParametersReply>
      result_reply;

  client_->GetFirmwareManagementParameters(
      ::user_data_auth::GetFirmwareManagementParametersRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_get_firmware_management_parameters_reply_));
}

TEST_F(InstallAttributesClientTest, BlockingInstallAttributesGet) {
  expected_blocking_install_attributes_get_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesGetReply> result_reply;

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingInstallAttributesGet(
      ::user_data_auth::InstallAttributesGetRequest());

  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_blocking_install_attributes_get_reply_));
}

TEST_F(InstallAttributesClientTest,
       BlockingInstallAttributesGetInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  absl::optional<::user_data_auth::InstallAttributesGetReply> result_reply =
      ::user_data_auth::InstallAttributesGetReply();

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingInstallAttributesGet(
      ::user_data_auth::InstallAttributesGetRequest());

  EXPECT_EQ(result_reply, absl::nullopt);
}

TEST_F(InstallAttributesClientTest, BlockingInstallAttributesSet) {
  expected_blocking_install_attributes_set_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesSetReply> result_reply;

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingInstallAttributesSet(
      ::user_data_auth::InstallAttributesSetRequest());

  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_blocking_install_attributes_set_reply_));
}

TEST_F(InstallAttributesClientTest, BlockingInstallAttributesFinalize) {
  expected_blocking_install_attributes_finalize_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesFinalizeReply> result_reply;

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingInstallAttributesFinalize(
      ::user_data_auth::InstallAttributesFinalizeRequest());

  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_blocking_install_attributes_finalize_reply_));
}

TEST_F(InstallAttributesClientTest, BlockingInstallAttributesGetStatus) {
  expected_blocking_install_attributes_get_status_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::InstallAttributesGetStatusReply>
      result_reply;

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingInstallAttributesGetStatus(
      ::user_data_auth::InstallAttributesGetStatusRequest());

  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_blocking_install_attributes_get_status_reply_));
}

}  // namespace ash
