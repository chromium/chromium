// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_driver_android.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/test/task_environment.h"
#include "components/fcm/fcm_app_handler.h"
#include "components/fcm/fcm_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fcm {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class MockFcmAppHandler : public FcmAppHandler {
 public:
  MOCK_METHOD(void, OnMessage, (const FcmMessage& message), (override));
  MOCK_METHOD(void, OnMessagesDeleted, (), (override));
  MOCK_METHOD(void,
              OnInstallationIdRefreshed,
              (const std::optional<InstallationId>& installation_id),
              (override));
};

class FcmDriverAndroidTest : public ::testing::Test {
 public:
  FcmDriverAndroidTest() = default;
  ~FcmDriverAndroidTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockFcmAppHandler> handler_;
};

TEST_F(FcmDriverAndroidTest, DispatchInstallationIdRefreshed) {
  FcmDriverAndroid driver;

  EXPECT_CALL(handler_, OnInstallationIdRefreshed(std::optional<InstallationId>(
                            InstallationId("test_fid_123"))));

  driver.AddAppHandler("test_app", &handler_);
  driver.OnInstallationIdRefreshed(base::android::AttachCurrentThread(),
                                   "test_fid_123");
}

TEST_F(FcmDriverAndroidTest, DispatchInstallationIdRefreshedEmpty) {
  FcmDriverAndroid driver;

  EXPECT_CALL(handler_, OnInstallationIdRefreshed(Eq(std::nullopt)));

  driver.AddAppHandler("test_app", &handler_);
  driver.OnInstallationIdRefreshed(base::android::AttachCurrentThread(), "");
}

TEST_F(FcmDriverAndroidTest, DispatchMessageReceived) {
  FcmDriverAndroid driver;

  driver.AddAppHandler("test_app", &handler_);

  std::vector<std::string> keys_and_values = {"subtype", "test_app", "key1",
                                              "val1",    "key2",     "val2"};
  std::vector<uint8_t> raw_data = {1, 2, 3, 4};

  EXPECT_CALL(
      handler_,
      OnMessage(AllOf(
          Field(&FcmMessage::message_id, "msg_id_1"),
          Field(
              &FcmMessage::data,
              UnorderedElementsAre(Pair("subtype", "test_app"),
                                   Pair("key1", "val1"), Pair("key2", "val2"))),
          Field(&FcmMessage::raw_data, std::string("\x01\x02\x03\x04", 4)))));

  driver.OnMessageReceived(base::android::AttachCurrentThread(), "msg_id_1",
                           keys_and_values, raw_data);
}

TEST_F(FcmDriverAndroidTest, DispatchMessagesDeleted) {
  FcmDriverAndroid driver;

  driver.AddAppHandler("test_app", &handler_);

  EXPECT_CALL(handler_, OnMessagesDeleted());

  driver.OnMessagesDeleted(base::android::AttachCurrentThread());
}

}  // namespace
}  // namespace fcm
