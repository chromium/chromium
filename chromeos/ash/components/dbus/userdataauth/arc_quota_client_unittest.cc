// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/arc_quota_client.h"

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

class ArcQuotaClientTest : public testing::Test {
 public:
  ArcQuotaClientTest() = default;
  ~ArcQuotaClientTest() override = default;

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
        .WillRepeatedly(Invoke(this, &ArcQuotaClientTest::OnCallMethod));

    ArcQuotaClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = ArcQuotaClient::Get();
  }

  void TearDown() override { ArcQuotaClient::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  raw_ptr<ArcQuotaClient, ExperimentalAsh> client_;

  // The expected replies to the respective D-Bus calls.
  ::user_data_auth::GetArcDiskFeaturesReply
      expected_get_arc_disk_features_reply_;
  ::user_data_auth::GetCurrentSpaceForArcUidReply
      expected_get_current_space_for_arc_uid_reply_;
  ::user_data_auth::GetCurrentSpaceForArcGidReply
      expected_get_current_space_for_arc_gid_reply_;
  ::user_data_auth::GetCurrentSpaceForArcProjectIdReply
      expected_get_current_space_for_arc_project_id_reply_;

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
               ::user_data_auth::kGetArcDiskFeatures) {
      writer.AppendProtoAsArrayOfBytes(expected_get_arc_disk_features_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetCurrentSpaceForArcUid) {
      writer.AppendProtoAsArrayOfBytes(
          expected_get_current_space_for_arc_uid_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetCurrentSpaceForArcGid) {
      writer.AppendProtoAsArrayOfBytes(
          expected_get_current_space_for_arc_gid_reply_);
    } else if (method_call->GetMember() ==
               ::user_data_auth::kGetCurrentSpaceForArcProjectId) {
      writer.AppendProtoAsArrayOfBytes(
          expected_get_current_space_for_arc_project_id_reply_);
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(RunResponseCallback, std::move(*callback),
                                  std::move(response)));
  }
};

TEST_F(ArcQuotaClientTest, GetArcDiskFeatures) {
  expected_get_arc_disk_features_reply_.set_quota_supported(true);
  absl::optional<::user_data_auth::GetArcDiskFeaturesReply> result_reply;

  client_->GetArcDiskFeatures(::user_data_auth::GetArcDiskFeaturesRequest(),
                              CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_get_arc_disk_features_reply_));
}

TEST_F(ArcQuotaClientTest, GetArcDiskFeaturesInvalidProtobuf) {
  expected_get_arc_disk_features_reply_.set_quota_supported(true);
  shall_message_parsing_fail_ = true;
  absl::optional<::user_data_auth::GetArcDiskFeaturesReply> result_reply =
      ::user_data_auth::GetArcDiskFeaturesReply();

  client_->GetArcDiskFeatures(::user_data_auth::GetArcDiskFeaturesRequest(),
                              CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_reply, absl::nullopt);
}

TEST_F(ArcQuotaClientTest, GetCurrentSpaceForArcUid) {
  constexpr int64_t kCurSpace = 0x12345678ABCDLL;
  expected_get_current_space_for_arc_uid_reply_.set_cur_space(kCurSpace);
  absl::optional<::user_data_auth::GetCurrentSpaceForArcUidReply> result_reply;

  client_->GetCurrentSpaceForArcUid(
      ::user_data_auth::GetCurrentSpaceForArcUidRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_get_current_space_for_arc_uid_reply_));
}

TEST_F(ArcQuotaClientTest, GetCurrentSpaceForArcGid) {
  constexpr int64_t kCurSpace = 0x12345678ABCDLL;
  expected_get_current_space_for_arc_gid_reply_.set_cur_space(kCurSpace);
  absl::optional<::user_data_auth::GetCurrentSpaceForArcGidReply> result_reply;

  client_->GetCurrentSpaceForArcGid(
      ::user_data_auth::GetCurrentSpaceForArcGidRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(ProtobufEquals(result_reply.value(),
                             expected_get_current_space_for_arc_gid_reply_));
}

TEST_F(ArcQuotaClientTest, GetCurrentSpaceForArcProjectId) {
  constexpr int64_t kCurSpace = 0x12345678ABCDLL;
  expected_get_current_space_for_arc_project_id_reply_.set_cur_space(kCurSpace);
  absl::optional<::user_data_auth::GetCurrentSpaceForArcProjectIdReply>
      result_reply;

  client_->GetCurrentSpaceForArcProjectId(
      ::user_data_auth::GetCurrentSpaceForArcProjectIdRequest(),
      CreateCopyCallback(&result_reply));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(result_reply, absl::nullopt);
  EXPECT_TRUE(
      ProtobufEquals(result_reply.value(),
                     expected_get_current_space_for_arc_project_id_reply_));
}

}  // namespace ash
