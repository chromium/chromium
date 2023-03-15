// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

#include <string>
#include <utility>

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

// Create a callback that would copy the input argument passed to it into |out|.
// This is used mostly to create a callback that would catch the reply from
// dbus.
template <typename T>
base::OnceCallback<void(T)> CreateCopyCallback(T* out) {
  return base::BindOnce([](T* out, T result) { *out = result; }, out);
}

class TestObserver : public UserDataAuthClient::Observer {
 public:
  // UserDataAuthClient::Observer overrides
  void LowDiskSpace(const ::user_data_auth::LowDiskSpace& status) override {
    last_low_disk_space_.CopyFrom(status);
    low_disk_space_count_++;
  }

  void DircryptoMigrationProgress(
      const ::user_data_auth::DircryptoMigrationProgress& progress) override {
    last_dircrypto_progress_.CopyFrom(progress);
    dircrypto_progress_count_++;
  }

  const ::user_data_auth::LowDiskSpace& last_low_disk_space() const {
    return last_low_disk_space_;
  }

  int low_disk_space_count() const { return low_disk_space_count_; }

  const ::user_data_auth::DircryptoMigrationProgress& last_dircrypto_progress()
      const {
    return last_dircrypto_progress_;
  }

  int dircrypto_progress_count() const { return dircrypto_progress_count_; }

 private:
  // The protobuf that came with the signal when the last low disk space_ event
  // came.
  ::user_data_auth::LowDiskSpace last_low_disk_space_;

  // The number of times the LowDiskSpace signal is triggered.
  int low_disk_space_count_ = 0;

  // The protobuf that came with the signal when the last dircrypto migration
  // progress event came.
  ::user_data_auth::DircryptoMigrationProgress last_dircrypto_progress_;

  // The number of times the DircryptoMigrationProgress signal is triggered.
  int dircrypto_progress_count_ = 0;
};

}  // namespace

class UserDataAuthClientTest : public testing::Test {
 public:
  UserDataAuthClientTest() = default;
  ~UserDataAuthClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    dbus::ObjectPath userdataauth_object_path =
        dbus::ObjectPath(::user_data_auth::kUserDataAuthServicePath);
    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), ::user_data_auth::kUserDataAuthServiceName,
        userdataauth_object_path);

    // Makes sure `GetObjectProxy()` is caled with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::user_data_auth::kUserDataAuthServiceName,
                               userdataauth_object_path))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_CALL(*proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(Invoke(this, &UserDataAuthClientTest::OnCallMethod));

    EXPECT_CALL(
        *proxy_,
        DoConnectToSignal(::user_data_auth::kUserDataAuthInterface,
                          ::user_data_auth::kDircryptoMigrationProgress, _, _))
        .WillOnce(SaveArg<2>(&dircrypto_progress_callback_));
    EXPECT_CALL(*proxy_,
                DoConnectToSignal(::user_data_auth::kUserDataAuthInterface,
                                  ::user_data_auth::kLowDiskSpace, _, _))
        .WillOnce(SaveArg<2>(&low_disk_space_callback_));

    EXPECT_CALL(*proxy_, DoConnectToSignal(
                             ::user_data_auth::kUserDataAuthInterface,
                             ::user_data_auth::kAuthScanResultSignal, _, _))
        .WillOnce(SaveArg<2>(&auth_scan_callback_));
    EXPECT_CALL(*proxy_,
                DoConnectToSignal(
                    ::user_data_auth::kUserDataAuthInterface,
                    ::user_data_auth::kAuthEnrollmentProgressSignal, _, _))
        .WillOnce(SaveArg<2>(&auth_enrollment_callback_));

    UserDataAuthClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = UserDataAuthClient::Get();
  }

  void TearDown() override { UserDataAuthClient::Shutdown(); }

 protected:
  void EmitLowDiskSpaceSignal(const ::user_data_auth::LowDiskSpace& status) {
    dbus::Signal signal(::user_data_auth::kUserDataAuthInterface,
                        ::user_data_auth::kLowDiskSpace);
    dbus::MessageWriter writer(&signal);
    writer.AppendProtoAsArrayOfBytes(status);
    // Emit the signal.
    ASSERT_FALSE(low_disk_space_callback_.is_null());
    low_disk_space_callback_.Run(&signal);
  }

  void EmitDircryptoMigrationProgressSignal(
      const ::user_data_auth::DircryptoMigrationProgress& status) {
    dbus::Signal signal(::user_data_auth::kUserDataAuthInterface,
                        ::user_data_auth::kDircryptoMigrationProgress);
    dbus::MessageWriter writer(&signal);
    writer.AppendProtoAsArrayOfBytes(status);
    // Emit the signal.
    ASSERT_FALSE(dircrypto_progress_callback_.is_null());
    dircrypto_progress_callback_.Run(&signal);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  UserDataAuthClient* client_;

  // The expected replies to the respective D-Bus calls.
  ::user_data_auth::IsMountedReply expected_is_mounted_reply_;
  ::user_data_auth::UnmountReply expected_unmount_reply_;
  ::user_data_auth::RemoveReply expected_remove_reply_;
  ::user_data_auth::CheckKeyReply expected_check_key_reply_;
  ::user_data_auth::StartMigrateToDircryptoReply
      expected_start_migrate_to_dircrypto_reply_;
  ::user_data_auth::NeedsDircryptoMigrationReply
      expected_needs_dircrypto_migration_reply_;
  ::user_data_auth::GetSupportedKeyPoliciesReply
      expected_get_supported_key_policies_reply_;
  ::user_data_auth::GetAccountDiskUsageReply
      expected_get_account_disk_usage_reply_;
  ::user_data_auth::StartAuthSessionReply expected_start_auth_session_reply_;

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
    } else if (method_call->GetMember() == ::user_data_auth::kIsMounted) {
      writer.AppendProtoAsArrayOfBytes(expected_is_mounted_reply_);
    } else if (method_call->GetMember() == ::user_data_auth::kIsMounted) {
      writer.AppendProtoAsArrayOfBytes(expected_is_mounted_reply_);
    } else if (method_call->GetMember() == ::user_data_auth::kUnmount) {
      writer.AppendProtoAsArrayOfBytes(expected_unmount_reply_);
    } else if (method_call->GetMember() == ::user_data_auth::kRemove) {
      writer.AppendProtoAsArrayOfBytes(expected_remove_reply_);
    } else if (method_call->GetMember() == ::user_data_auth::kCheckKey) {
      writer.AppendProtoAsArrayOfBytes(expected_check_key_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kStartMigrateToDircrypto) {
      writer.AppendProtoAsArrayOfBytes(
          expected_start_migrate_to_dircrypto_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kNeedsDircryptoMigration) {
      writer.AppendProtoAsArrayOfBytes(
          expected_needs_dircrypto_migration_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetSupportedKeyPolicies) {
      writer.AppendProtoAsArrayOfBytes(
          expected_get_supported_key_policies_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetAccountDiskUsage) {
      writer.AppendProtoAsArrayOfBytes(expected_get_account_disk_usage_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kStartAuthSession) {
      writer.AppendProtoAsArrayOfBytes(expected_start_auth_session_reply_);
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }

  // Callback that delivers the Low Disk Space signal to the client when called.
  dbus::ObjectProxy::SignalCallback low_disk_space_callback_;

  // Callback that delivers the dircrypto Migration Progress signal to the
  // client when called.
  dbus::ObjectProxy::SignalCallback dircrypto_progress_callback_;

  // Callback that delivers the cryptohome AuthScanResult signal to the
  // client when called.
  dbus::ObjectProxy::SignalCallback auth_scan_callback_;

  // Callback that delivers the cryptohome AuthEnrollmentProgress signal to the
  // client when called.
  dbus::ObjectProxy::SignalCallback auth_enrollment_callback_;
};

TEST_F(UserDataAuthClientTest, IsMounted) {
  expected_is_mounted_reply_.set_is_mounted(true);
  expected_is_mounted_reply_.set_is_ephemeral_mount(false);
  absl::optional<::user_data_auth::IsMountedReply> result_reply = absl::nullopt;
  auto callback = base::BindOnce(
      [](absl::optional<::user_data_auth::IsMountedReply>* result_reply,
         absl::optional<::user_data_auth::IsMountedReply> reply) {
        *result_reply = reply;
      },
      &result_reply);

  client_->IsMounted(::user_data_auth::IsMountedRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(result_reply.value().is_mounted());
  EXPECT_FALSE(result_reply.value().is_ephemeral_mount());
}

TEST_F(UserDataAuthClientTest, IsMountedInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  absl::optional<::user_data_auth::IsMountedReply> result_reply =
      ::user_data_auth::IsMountedReply();
  auto callback = base::BindOnce(
      [](absl::optional<::user_data_auth::IsMountedReply>* result_reply,
         absl::optional<::user_data_auth::IsMountedReply> reply) {
        *result_reply = reply;
      },
      &result_reply);

  client_->IsMounted(::user_data_auth::IsMountedRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_reply, absl::nullopt);
}

TEST_F(UserDataAuthClientTest, Unmount) {
  expected_unmount_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::UnmountReply> result_reply;

  client_->Unmount(::user_data_auth::UnmountRequest(),
                   CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(), expected_unmount_reply_));
}

TEST_F(UserDataAuthClientTest, Remove) {
  expected_remove_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::RemoveReply> result_reply;

  client_->Remove(::user_data_auth::RemoveRequest(),
                  CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(), expected_remove_reply_));
}

TEST_F(UserDataAuthClientTest, CheckKey) {
  expected_check_key_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::CheckKeyReply> result_reply;

  client_->CheckKey(::user_data_auth::CheckKeyRequest(),
                    CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(), expected_check_key_reply_));
}

TEST_F(UserDataAuthClientTest, StartMigrateToDircrypto) {
  expected_start_migrate_to_dircrypto_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::StartMigrateToDircryptoReply> result_reply;

  client_->StartMigrateToDircrypto(
      ::user_data_auth::StartMigrateToDircryptoRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_start_migrate_to_dircrypto_reply_));
}

TEST_F(UserDataAuthClientTest, NeedsDircryptoMigration) {
  expected_needs_dircrypto_migration_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::NeedsDircryptoMigrationReply> result_reply;

  client_->NeedsDircryptoMigration(
      ::user_data_auth::NeedsDircryptoMigrationRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_needs_dircrypto_migration_reply_));
}

TEST_F(UserDataAuthClientTest, GetSupportedKeyPolicies) {
  expected_get_supported_key_policies_reply_
      .set_low_entropy_credentials_supported(true);
  absl::optional<::user_data_auth::GetSupportedKeyPoliciesReply> result_reply;

  client_->GetSupportedKeyPolicies(
      ::user_data_auth::GetSupportedKeyPoliciesRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_get_supported_key_policies_reply_));
}

TEST_F(UserDataAuthClientTest, GetAccountDiskUsage) {
  expected_get_account_disk_usage_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::GetAccountDiskUsageReply> result_reply;

  client_->GetAccountDiskUsage(::user_data_auth::GetAccountDiskUsageRequest(),
                               CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_get_account_disk_usage_reply_));
}

TEST_F(UserDataAuthClientTest, StartAuthSession) {
  expected_start_auth_session_reply_.set_error(
      user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
  absl::optional<::user_data_auth::StartAuthSessionReply> result_reply;

  client_->StartAuthSession(::user_data_auth::StartAuthSessionRequest(),
                            CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(), expected_start_auth_session_reply_));
}

TEST_F(UserDataAuthClientTest, LowDiskSpaceSignal) {
  constexpr uint64_t kFreeSpace1 = 0x1234567890123ULL;
  constexpr uint64_t kFreeSpace2 = 0xFFFF9876ULL;

  TestObserver observer;
  client_->AddObserver(&observer);

  ::user_data_auth::LowDiskSpace status;
  status.set_disk_free_bytes(kFreeSpace1);

  // Basic validity check, emit a signal and check.
  EmitLowDiskSpaceSignal(status);
  EXPECT_EQ(observer.low_disk_space_count(), 1);
  EXPECT_EQ(observer.last_low_disk_space().disk_free_bytes(), kFreeSpace1);

  // Try again to see nothing is stuck.
  status.set_disk_free_bytes(kFreeSpace2);
  EmitLowDiskSpaceSignal(status);
  EXPECT_EQ(observer.low_disk_space_count(), 2);
  EXPECT_EQ(observer.last_low_disk_space().disk_free_bytes(), kFreeSpace2);

  // Remove the observer to check that it no longer gets triggered.
  client_->RemoveObserver(&observer);
  EmitLowDiskSpaceSignal(status);
  EXPECT_EQ(observer.low_disk_space_count(), 2);
}

TEST_F(UserDataAuthClientTest, DircryptoMigrationProgressSignal) {
  // Prepare the test constants.
  constexpr uint64_t kTotalBytes = 0x1234567890123ULL;
  ::user_data_auth::DircryptoMigrationProgress progress1, progress2;
  progress1.set_status(::user_data_auth::DircryptoMigrationStatus::
                           DIRCRYPTO_MIGRATION_IN_PROGRESS);
  progress1.set_current_bytes(12345);
  progress1.set_total_bytes(kTotalBytes);
  progress2.set_status(
      ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS);
  progress2.set_current_bytes(kTotalBytes);
  progress2.set_total_bytes(kTotalBytes);

  TestObserver observer;
  client_->AddObserver(&observer);

  // Basic validity check, emit a signal and check.
  EmitDircryptoMigrationProgressSignal(progress1);
  EXPECT_EQ(observer.dircrypto_progress_count(), 1);
  EXPECT_TRUE(ProtobufEquals(observer.last_dircrypto_progress(), progress1));

  // Try again to see nothing is stuck.
  EmitDircryptoMigrationProgressSignal(progress2);
  EXPECT_EQ(observer.dircrypto_progress_count(), 2);
  EXPECT_TRUE(ProtobufEquals(observer.last_dircrypto_progress(), progress2));

  // Remove the observer to check that it no longer gets triggered.
  client_->RemoveObserver(&observer);
  EmitDircryptoMigrationProgressSignal(progress1);
  EXPECT_EQ(observer.dircrypto_progress_count(), 2);
}

}  // namespace ash
