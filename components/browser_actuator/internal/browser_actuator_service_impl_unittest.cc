// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include "components/browser_actuator/test_support/transport/mock_transport_channel.h"
#include "components/browser_actuator/test_support/transport/mock_transport_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using ::testing::Return;

TEST(BrowserActuatorServiceImplTest, IsInitialized) {
  BrowserActuatorServiceImpl service(/*transport_service=*/nullptr);
  EXPECT_TRUE(service.IsInitialized());
}

TEST(BrowserActuatorServiceImplTest, ForwardsToTransportService) {
  transport::MockTransportService transport_service;
  transport::MockTransportChannel channel;
  EXPECT_CALL(transport_service, GetChannel()).WillOnce(Return(&channel));
  EXPECT_CALL(transport_service, GetOrCreateSession("s1"))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(transport_service, GetSession("s1")).WillOnce(Return(nullptr));
  EXPECT_CALL(transport_service, GetStreamRecorderFactory())
      .WillOnce(Return(nullptr));

  BrowserActuatorServiceImpl service(&transport_service);

  EXPECT_EQ(service.GetChannel(), &channel);
  EXPECT_EQ(service.GetOrCreateSession("s1"), nullptr);
  EXPECT_EQ(service.GetSession("s1"), nullptr);
  EXPECT_EQ(service.GetStreamRecorderFactory(), nullptr);
}

TEST(BrowserActuatorServiceImplTest, ReturnsNullWithoutTransportService) {
  BrowserActuatorServiceImpl service(/*transport_service=*/nullptr);

  EXPECT_EQ(service.GetChannel(), nullptr);
  EXPECT_EQ(service.GetOrCreateSession("s1"), nullptr);
  EXPECT_EQ(service.GetSession("s1"), nullptr);
  EXPECT_EQ(service.GetStreamRecorderFactory(), nullptr);
}

}  // namespace
}  // namespace browser_actuator
