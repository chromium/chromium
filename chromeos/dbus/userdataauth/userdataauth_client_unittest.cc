// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/userdataauth_client.h"

#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

// Runs |callback| with |response|. Needed due to ResponseCallback expecting a
// bare pointer rather than an std::unique_ptr.
void RunResponseCallback(dbus::ObjectProxy::ResponseCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get());
}

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

    UserDataAuthClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = UserDataAuthClient::Get();
  }

  void TearDown() override { UserDataAuthClient::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  UserDataAuthClient* client_;

  // The expected replies to the respective D-Bus calls.
  ::user_data_auth::IsMountedReply expected_is_mounted_reply_;

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
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }
};

TEST_F(UserDataAuthClientTest, IsMounted) {
  expected_is_mounted_reply_.set_is_mounted(true);
  expected_is_mounted_reply_.set_is_ephemeral_mount(false);
  base::Optional<::user_data_auth::IsMountedReply> result_reply = base::nullopt;
  auto callback = base::BindOnce(
      [](base::Optional<::user_data_auth::IsMountedReply>* result_reply,
         base::Optional<::user_data_auth::IsMountedReply> reply) {
        *result_reply = reply;
      },
      &result_reply);

  client_->IsMounted(::user_data_auth::IsMountedRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, base::nullopt);
  EXPECT_TRUE(result_reply.value().is_mounted());
  EXPECT_FALSE(result_reply.value().is_ephemeral_mount());
}

TEST_F(UserDataAuthClientTest, IsMountedInvalidProtobuf) {
  shall_message_parsing_fail_ = true;
  base::Optional<::user_data_auth::IsMountedReply> result_reply =
      ::user_data_auth::IsMountedReply();
  auto callback = base::BindOnce(
      [](base::Optional<::user_data_auth::IsMountedReply>* result_reply,
         base::Optional<::user_data_auth::IsMountedReply> reply) {
        *result_reply = reply;
      },
      &result_reply);

  client_->IsMounted(::user_data_auth::IsMountedRequest(), std::move(callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_reply, base::nullopt);
}

}  // namespace chromeos
