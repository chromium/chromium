// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/cryptohome_pkcs11_client.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
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

}  // namespace

class CryptohomePkcs11ClientTest : public testing::Test {
 public:
  CryptohomePkcs11ClientTest() = default;
  ~CryptohomePkcs11ClientTest() override = default;

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
            Invoke(this, &CryptohomePkcs11ClientTest::OnCallMethod));

    CryptohomePkcs11Client::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = CryptohomePkcs11Client::Get();
  }

  void TearDown() override { CryptohomePkcs11Client::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  raw_ptr<CryptohomePkcs11Client, DanglingUntriaged> client_;

  // The expected replies to the respective D-Bus calls.
  ::user_data_auth::Pkcs11IsTpmTokenReadyReply
      expected_pkcs11_is_tpm_token_ready_reply_;
  ::user_data_auth::Pkcs11GetTpmTokenInfoReply
      expected_pkcs11_get_tpm_token_info_reply_;

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
    } else if (method_call->GetMember() ==
               ::user_data_auth::kPkcs11IsTpmTokenReady) {
      writer.AppendProtoAsArrayOfBytes(
          expected_pkcs11_is_tpm_token_ready_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kPkcs11GetTpmTokenInfo) {
      writer.AppendProtoAsArrayOfBytes(
          expected_pkcs11_get_tpm_token_info_reply_);
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }
};

TEST_F(CryptohomePkcs11ClientTest, Pkcs11IsTpmTokenReadyInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  std::optional<::user_data_auth::Pkcs11IsTpmTokenReadyReply> result_reply =
      ::user_data_auth::Pkcs11IsTpmTokenReadyReply();

  client_->Pkcs11IsTpmTokenReady(
      ::user_data_auth::Pkcs11IsTpmTokenReadyRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_reply, std::nullopt);
}

TEST_F(CryptohomePkcs11ClientTest, Pkcs11IsTpmTokenReady) {
  expected_pkcs11_is_tpm_token_ready_reply_.set_ready(true);
  std::optional<::user_data_auth::Pkcs11IsTpmTokenReadyReply> result_reply;

  client_->Pkcs11IsTpmTokenReady(
      ::user_data_auth::Pkcs11IsTpmTokenReadyRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_pkcs11_is_tpm_token_ready_reply_));
}

TEST_F(CryptohomePkcs11ClientTest, Pkcs11GetTpmTokenInfo) {
  constexpr int kSlot = 42;
  expected_pkcs11_get_tpm_token_info_reply_.mutable_token_info()->set_slot(
      kSlot);
  std::optional<::user_data_auth::Pkcs11GetTpmTokenInfoReply> result_reply;

  client_->Pkcs11GetTpmTokenInfo(
      ::user_data_auth::Pkcs11GetTpmTokenInfoRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, std::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_pkcs11_get_tpm_token_info_reply_));
}

}  // namespace ash
