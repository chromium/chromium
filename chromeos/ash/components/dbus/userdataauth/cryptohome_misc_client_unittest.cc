// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"

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

class CryptohomeMiscClientTest : public testing::Test {
 public:
  CryptohomeMiscClientTest() = default;
  ~CryptohomeMiscClientTest() override = default;

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
        .WillRepeatedly(Invoke(this, &CryptohomeMiscClientTest::OnCallMethod));
    EXPECT_CALL(*proxy_.get(), CallMethodAndBlock(_, _))
        .WillRepeatedly(
            Invoke(this, &CryptohomeMiscClientTest::OnBlockingCallMethod));

    CryptohomeMiscClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = CryptohomeMiscClient::Get();
  }

  void TearDown() override { CryptohomeMiscClient::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  raw_ptr<CryptohomeMiscClient, DanglingUntriaged> client_;

  // The expected replies to the respective D-Bus calls.
  ::user_data_auth::GetSystemSaltReply expected_get_system_salt_reply_;
  ::user_data_auth::GetSanitizedUsernameReply
      expected_get_sanitized_username_reply_;
  ::user_data_auth::GetLoginStatusReply expected_get_login_status_reply_;
  ::user_data_auth::LockToSingleUserMountUntilRebootReply
      expected_lock_to_single_user_mount_until_reboot_reply_;
  ::user_data_auth::GetRsuDeviceIdReply expected_get_rsu_device_id_reply_;

  // The expected replies to the respective blocking D-Bus calls.
  ::user_data_auth::GetSanitizedUsernameReply
      expected_blocking_get_sanitized_username_reply_;

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
      writer.AppendArrayOfBytes(invalid_protobuf);
    } else if (method_call->GetMember() == ::user_data_auth::kGetSystemSalt) {
      writer.AppendProtoAsArrayOfBytes(expected_get_system_salt_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetSanitizedUsername) {
      writer.AppendProtoAsArrayOfBytes(expected_get_sanitized_username_reply_);
    } else if (method_call->GetMember() == ::user_data_auth::kGetLoginStatus) {
      writer.AppendProtoAsArrayOfBytes(expected_get_login_status_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kLockToSingleUserMountUntilReboot) {
      writer.AppendProtoAsArrayOfBytes(
          expected_lock_to_single_user_mount_until_reboot_reply_);
    } else if (method_call->GetMember() == ::user_data_auth::kGetRsuDeviceId) {
      writer.AppendProtoAsArrayOfBytes(expected_get_rsu_device_id_reply_);
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
      writer.AppendArrayOfBytes(invalid_protobuf);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetSanitizedUsername) {
      writer.AppendProtoAsArrayOfBytes(
          expected_blocking_get_sanitized_username_reply_);
    } else {
      LOG(FATAL) << "Unrecognized member: " << method_call->GetMember();
    }
    return base::ok(std::move(response));
  }
};

TEST_F(CryptohomeMiscClientTest, GetSystemSalt) {
  constexpr char kSalt[] = "example_salt";
  expected_get_system_salt_reply_.set_salt(std::string(kSalt));
  std::optional<::user_data_auth::GetSystemSaltReply> result_reply;

  client_->GetSystemSalt(::user_data_auth::GetSystemSaltRequest(),
                         CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(), expected_get_system_salt_reply_));
}

TEST_F(CryptohomeMiscClientTest, GetSystemSaltInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  std::optional<::user_data_auth::GetSystemSaltReply> result_reply =
      ::user_data_auth::GetSystemSaltReply();

  client_->GetSystemSalt(::user_data_auth::GetSystemSaltRequest(),
                         CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_reply, std::nullopt);
}

TEST_F(CryptohomeMiscClientTest, GetSanitizedUsername) {
  constexpr char kAccountId[] = "test1234@example.com";
  expected_get_sanitized_username_reply_.set_sanitized_username(
      std::string(kAccountId));
  std::optional<::user_data_auth::GetSanitizedUsernameReply> result_reply;

  client_->GetSanitizedUsername(::user_data_auth::GetSanitizedUsernameRequest(),
                                CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_get_sanitized_username_reply_));
}

TEST_F(CryptohomeMiscClientTest, GetLoginStatus) {
  expected_get_login_status_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  std::optional<::user_data_auth::GetLoginStatusReply> result_reply;

  client_->GetLoginStatus(::user_data_auth::GetLoginStatusRequest(),
                          CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(), expected_get_login_status_reply_));
}

TEST_F(CryptohomeMiscClientTest, LockToSingleUserMountUntilReboot) {
  expected_lock_to_single_user_mount_until_reboot_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  std::optional<::user_data_auth::LockToSingleUserMountUntilRebootReply>
      result_reply;

  client_->LockToSingleUserMountUntilReboot(
      ::user_data_auth::LockToSingleUserMountUntilRebootRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_lock_to_single_user_mount_until_reboot_reply_));
}

TEST_F(CryptohomeMiscClientTest, GetRsuDeviceId) {
  expected_get_rsu_device_id_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  std::optional<::user_data_auth::GetRsuDeviceIdReply> result_reply;

  client_->GetRsuDeviceId(::user_data_auth::GetRsuDeviceIdRequest(),
                          CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(), expected_get_rsu_device_id_reply_));
}

TEST_F(CryptohomeMiscClientTest, BlockingGetSanitizedUsername) {
  constexpr char kAccountId[] = "test1234@example.com";
  expected_blocking_get_sanitized_username_reply_.set_sanitized_username(
      std::string(kAccountId));
  std::optional<::user_data_auth::GetSanitizedUsernameReply> result_reply;

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingGetSanitizedUsername(
      ::user_data_auth::GetSanitizedUsernameRequest());

  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_blocking_get_sanitized_username_reply_));
}

TEST_F(CryptohomeMiscClientTest, BlockingGetSanitizedUsernameInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  std::optional<::user_data_auth::GetSanitizedUsernameReply> result_reply =
      ::user_data_auth::GetSanitizedUsernameReply();

  scoped_refptr<FakeTaskRunner> runner = new FakeTaskRunner;
  EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(runner.get()));

  result_reply = client_->BlockingGetSanitizedUsername(
      ::user_data_auth::GetSanitizedUsernameRequest());

  EXPECT_EQ(result_reply, std::nullopt);
}

}  // namespace ash
