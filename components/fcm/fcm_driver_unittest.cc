// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_driver.h"

#include <memory>
#include <string>

#include "components/fcm/fcm_app_handler.h"
#include "components/fcm/fcm_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fcm {
namespace {

using ::testing::_;
using ::testing::NiceMock;

class MockFcmAppHandler : public FcmAppHandler {
 public:
  MOCK_METHOD(void, OnMessage, (const FcmMessage& message), (override));
  MOCK_METHOD(void, OnMessagesDeleted, (), (override));
  MOCK_METHOD(void,
              OnInstallationIdRefreshed,
              (const std::optional<InstallationId>& installation_id),
              (override));
};

class FcmDriverTest : public ::testing::Test {
 public:
  FcmDriverTest() = default;
  ~FcmDriverTest() override = default;

 protected:
  FcmDriver driver_;
  NiceMock<MockFcmAppHandler> handler_;
};

TEST_F(FcmDriverTest, AddAndRemoveAppHandler) {
  // This test currently does not verify anything and is used to check build
  // only.
  driver_.AddAppHandler("test_app", &handler_);
  driver_.RemoveAppHandler("test_app", &handler_);
}

}  // namespace
}  // namespace fcm
