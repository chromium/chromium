// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/tpm_manager/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

namespace {

// Runs |callback| with |response|. Needed due to ResponseCallback expecting a
// bare pointer rather than an std::unique_ptr.
void RunResponseCallback(dbus::ObjectProxy::ResponseCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get());
}

// The observer class used for testing to watch the invocation of the signal
// callbacks.
class TestObserver : public TpmManagerClient::Observer {
 public:
  // TpmManagerClient::Observer.
  void OnOwnershipTaken() override { ++signal_count_; }

  int signal_count() const { return signal_count_; }

 private:
  int signal_count_ = 0;
};

}  // namespace

class TpmManagerClientTest : public testing::Test {
 public:
  TpmManagerClientTest() = default;
  ~TpmManagerClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    dbus::ObjectPath tpm_manager_object_path =
        dbus::ObjectPath(::tpm_manager::kTpmManagerServicePath);
    proxy_ = new dbus::MockObjectProxy(bus_.get(),
                                       ::tpm_manager::kTpmManagerServiceName,
                                       tpm_manager_object_path);

    // Makes sure `GetObjectProxy()` is caled with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::tpm_manager::kTpmManagerServiceName,
                               tpm_manager_object_path))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_CALL(*proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(Invoke(this, &TpmManagerClientTest::OnCallMethod));

    EXPECT_CALL(*proxy_,
                DoConnectToSignal(::tpm_manager::kTpmManagerInterface,
                                  ::tpm_manager::kOwnershipTakenSignal, _, _))
        .WillOnce(SaveArg<2>(&ownership_taken_signal_callback_));
    TpmManagerClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    ASSERT_FALSE(ownership_taken_signal_callback_.is_null());

    client_ = TpmManagerClient::Get();
  }

  void TearDown() override { TpmManagerClient::Shutdown(); }

 protected:
  void EmitOwnershipTakenSignal() {
    ::tpm_manager::OwnershipTakenSignal ownership_taken_signal;
    dbus::Signal signal(::tpm_manager::kTpmManagerInterface,
                        ::tpm_manager::kOwnershipTakenSignal);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(
        ownership_taken_signal);
    // Emit signal.
    ASSERT_FALSE(ownership_taken_signal_callback_.is_null());
    ownership_taken_signal_callback_.Run(&signal);
  }
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  raw_ptr<TpmManagerClient, DanglingUntriaged> client_;

  // The expected replies to the respective D-Bus calls.
  ::tpm_manager::GetTpmNonsensitiveStatusReply expected_status_reply_;
  ::tpm_manager::GetVersionInfoReply expected_version_info_reply_;
  ::tpm_manager::GetSupportedFeaturesReply expected_supported_features_reply_;
  ::tpm_manager::GetDictionaryAttackInfoReply expected_get_da_info_reply_;
  ::tpm_manager::TakeOwnershipReply expected_take_ownership_reply_;
  ::tpm_manager::ClearStoredOwnerPasswordReply expected_clear_password_reply_;
  ::tpm_manager::ClearTpmReply expected_clear_tpm_reply_;

  // When it is set `true`, the parsing failure is expected to be translated by
  // proxy to status `STATUS_DBUS_ERROR`.
  bool shall_message_parsing_fail_ = false;

 private:
  // Handles calls to |proxy_|'s `CallMethod()`.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* callback) {
    std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    if (shall_message_parsing_fail_) {
      // Append anything but a valid string that can be deserialized to any
      // protobuf messages that tpm manager could possibly sends. A numerical
      // type is chosen instead of a string in case the string can actually be
      // parsed into any ptotobuf message unexpectedly.
      writer.AppendUint32(0);
    } else if (method_call->GetMember() ==
               ::tpm_manager::kGetTpmNonsensitiveStatus) {
      writer.AppendProtoAsArrayOfBytes(expected_status_reply_);
    } else if (method_call->GetMember() == ::tpm_manager::kGetVersionInfo) {
      writer.AppendProtoAsArrayOfBytes(expected_version_info_reply_);
    } else if (method_call->GetMember() ==
               ::tpm_manager::kGetSupportedFeatures) {
      writer.AppendProtoAsArrayOfBytes(expected_supported_features_reply_);
    } else if (method_call->GetMember() ==
               ::tpm_manager::kGetDictionaryAttackInfo) {
      writer.AppendProtoAsArrayOfBytes(expected_get_da_info_reply_);
    } else if (method_call->GetMember() == ::tpm_manager::kTakeOwnership) {
      writer.AppendProtoAsArrayOfBytes(expected_take_ownership_reply_);
    } else if (method_call->GetMember() ==
               ::tpm_manager::kClearStoredOwnerPassword) {
      writer.AppendProtoAsArrayOfBytes(expected_clear_password_reply_);
    } else if (method_call->GetMember() == ::tpm_manager::kClearTpm) {
      writer.AppendProtoAsArrayOfBytes(expected_clear_tpm_reply_);
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }

  dbus::ObjectProxy::SignalCallback ownership_taken_signal_callback_;
};

TEST_F(TpmManagerClientTest, GetTpmNonsensitiveStatus) {
  expected_status_reply_.set_status(::tpm_manager::STATUS_SUCCESS);
  expected_status_reply_.set_is_owned(true);
  expected_status_reply_.set_is_enabled(true);
  ::tpm_manager::GetTpmNonsensitiveStatusReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetTpmNonsensitiveStatusReply* result_reply,
         const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_status_reply_.status(), result_reply.status());
  EXPECT_EQ(expected_status_reply_.is_owned(), result_reply.is_owned());
  EXPECT_EQ(expected_status_reply_.is_enabled(), result_reply.is_enabled());
}

TEST_F(TpmManagerClientTest, GetTpmNonsensitiveStatusDBusFailure) {
  shall_message_parsing_fail_ = true;
  ::tpm_manager::GetTpmNonsensitiveStatusReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetTpmNonsensitiveStatusReply* result_reply,
         const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(::tpm_manager::STATUS_DBUS_ERROR, result_reply.status());
}

TEST_F(TpmManagerClientTest, GetVersionInfo) {
  expected_version_info_reply_.set_status(::tpm_manager::STATUS_SUCCESS);
  expected_version_info_reply_.set_tpm_model(123);
  expected_version_info_reply_.set_family(456);
  ::tpm_manager::GetVersionInfoReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetVersionInfoReply* result_reply,
         const ::tpm_manager::GetVersionInfoReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetVersionInfo(::tpm_manager::GetVersionInfoRequest(),
                          std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_version_info_reply_.status(), result_reply.status());
  EXPECT_EQ(expected_version_info_reply_.family(), result_reply.family());
  EXPECT_EQ(expected_version_info_reply_.tpm_model(), result_reply.tpm_model());
}

TEST_F(TpmManagerClientTest, GetVersionInfoDBusFailure) {
  shall_message_parsing_fail_ = true;
  ::tpm_manager::GetVersionInfoReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetVersionInfoReply* result_reply,
         const ::tpm_manager::GetVersionInfoReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetVersionInfo(::tpm_manager::GetVersionInfoRequest(),
                          std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(::tpm_manager::STATUS_DBUS_ERROR, result_reply.status());
}

TEST_F(TpmManagerClientTest, GetSupportedFeatures) {
  expected_supported_features_reply_.set_status(::tpm_manager::STATUS_SUCCESS);
  expected_supported_features_reply_.set_support_u2f(true);
  ::tpm_manager::GetSupportedFeaturesReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetSupportedFeaturesReply* result_reply,
         const ::tpm_manager::GetSupportedFeaturesReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetSupportedFeatures(::tpm_manager::GetSupportedFeaturesRequest(),
                                std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_supported_features_reply_.status(), result_reply.status());
  EXPECT_EQ(expected_supported_features_reply_.support_u2f(),
            result_reply.support_u2f());
}

TEST_F(TpmManagerClientTest, GetSupportedFeaturesDBusFailure) {
  shall_message_parsing_fail_ = true;
  ::tpm_manager::GetSupportedFeaturesReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetSupportedFeaturesReply* result_reply,
         const ::tpm_manager::GetSupportedFeaturesReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetSupportedFeatures(::tpm_manager::GetSupportedFeaturesRequest(),
                                std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(::tpm_manager::STATUS_DBUS_ERROR, result_reply.status());
}

TEST_F(TpmManagerClientTest, GetDictionaryAttackInfo) {
  expected_get_da_info_reply_.set_status(::tpm_manager::STATUS_SUCCESS);
  expected_get_da_info_reply_.set_dictionary_attack_counter(123);
  expected_get_da_info_reply_.set_dictionary_attack_lockout_in_effect(true);
  ::tpm_manager::GetDictionaryAttackInfoReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetDictionaryAttackInfoReply* result_reply,
         const ::tpm_manager::GetDictionaryAttackInfoReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetDictionaryAttackInfo(
      ::tpm_manager::GetDictionaryAttackInfoRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_get_da_info_reply_.status(), result_reply.status());
  EXPECT_EQ(expected_get_da_info_reply_.dictionary_attack_counter(),
            result_reply.dictionary_attack_counter());
  EXPECT_EQ(expected_get_da_info_reply_.dictionary_attack_lockout_in_effect(),
            result_reply.dictionary_attack_lockout_in_effect());
}

TEST_F(TpmManagerClientTest, GetDictionaryAttackInfoDBusFailure) {
  shall_message_parsing_fail_ = true;
  ::tpm_manager::GetDictionaryAttackInfoReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::GetDictionaryAttackInfoReply* result_reply,
         const ::tpm_manager::GetDictionaryAttackInfoReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->GetDictionaryAttackInfo(
      ::tpm_manager::GetDictionaryAttackInfoRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(::tpm_manager::STATUS_DBUS_ERROR, result_reply.status());
}

TEST_F(TpmManagerClientTest, TakeOwnership) {
  // Use a non-zero status value to make sure the value is correctly set.
  expected_take_ownership_reply_.set_status(::tpm_manager::STATUS_DEVICE_ERROR);
  ::tpm_manager::TakeOwnershipReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::TakeOwnershipReply* result_reply,
         const ::tpm_manager::TakeOwnershipReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->TakeOwnership(::tpm_manager::TakeOwnershipRequest(),
                         std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_take_ownership_reply_.status(), result_reply.status());
}

TEST_F(TpmManagerClientTest, TakeOwnershipDBusFailure) {
  shall_message_parsing_fail_ = true;
  ::tpm_manager::TakeOwnershipReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::TakeOwnershipReply* result_reply,
         const ::tpm_manager::TakeOwnershipReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->TakeOwnership(::tpm_manager::TakeOwnershipRequest(),
                         std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(::tpm_manager::STATUS_DBUS_ERROR, result_reply.status());
}

TEST_F(TpmManagerClientTest, ClearStoredOwnerPassword) {
  // Use a non-zero status value to make sure the value is correctly set.
  expected_clear_password_reply_.set_status(::tpm_manager::STATUS_DEVICE_ERROR);
  ::tpm_manager::ClearStoredOwnerPasswordReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::ClearStoredOwnerPasswordReply* result_reply,
         const ::tpm_manager::ClearStoredOwnerPasswordReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->ClearStoredOwnerPassword(
      ::tpm_manager::ClearStoredOwnerPasswordRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_clear_password_reply_.status(), result_reply.status());
}

TEST_F(TpmManagerClientTest, ClearTpm) {
  // Use a non-zero status value to make sure the value is correctly set.
  expected_clear_password_reply_.set_status(::tpm_manager::STATUS_DEVICE_ERROR);
  ::tpm_manager::ClearTpmReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::ClearTpmReply* result_reply,
         const ::tpm_manager::ClearTpmReply& reply) { *result_reply = reply; },
      &result_reply);
  client_->ClearTpm(::tpm_manager::ClearTpmRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_clear_tpm_reply_.status(), result_reply.status());
}

TEST_F(TpmManagerClientTest, OnwershipTakenSignal) {
  TestObserver observer;
  ASSERT_EQ(observer.signal_count(), 0);

  client_->AddObserver(&observer);
  EmitOwnershipTakenSignal();

  EXPECT_EQ(observer.signal_count(), 1);

  client_->RemoveObserver(&observer);
  EmitOwnershipTakenSignal();

  EXPECT_EQ(observer.signal_count(), 1);
}

TEST_F(TpmManagerClientTest, ClearStoredOwnerPasswordDBusFailure) {
  shall_message_parsing_fail_ = true;
  ::tpm_manager::ClearStoredOwnerPasswordReply result_reply;
  auto callback = base::BindOnce(
      [](::tpm_manager::ClearStoredOwnerPasswordReply* result_reply,
         const ::tpm_manager::ClearStoredOwnerPasswordReply& reply) {
        *result_reply = reply;
      },
      &result_reply);
  client_->ClearStoredOwnerPassword(
      ::tpm_manager::ClearStoredOwnerPasswordRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(::tpm_manager::STATUS_DBUS_ERROR, result_reply.status());
}

}  // namespace chromeos
