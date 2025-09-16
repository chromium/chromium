// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"

#include <string>

#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

struct StateStringProtoPair {
  std::string state_string;
  ::boca::ReceiverConnectionState state_proto;
};

using KioskReceiverParserTest = testing::TestWithParam<StateStringProtoPair>;

TEST_F(KioskReceiverParserTest, ProtoFromJsonHandlesUnknownString) {
  EXPECT_EQ(ReceiverConnectionStateProtoFromJson("some_unknown_state"),
            ::boca::ReceiverConnectionState::RECEIVER_CONNECTION_STATE_UNKNOWN);
}

TEST_P(KioskReceiverParserTest, ReceiverConnectionStateProtoFromJson) {
  const StateStringProtoPair& pair = GetParam();
  EXPECT_EQ(ReceiverConnectionStateProtoFromJson(pair.state_string),
            pair.state_proto);
}

TEST_P(KioskReceiverParserTest, ReceiverConnectionStateStringFromProto) {
  const StateStringProtoPair& pair = GetParam();
  EXPECT_EQ(ReceiverConnectionStateStringFromProto(pair.state_proto),
            pair.state_string);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskReceiverParserTest,
    testing::Values(
        StateStringProtoPair{
            "RECEIVER_STATE_UNKNOWN",
            ::boca::ReceiverConnectionState::RECEIVER_CONNECTION_STATE_UNKNOWN},
        StateStringProtoPair{"START_REQUESTED",
                             ::boca::ReceiverConnectionState::START_REQUESTED},
        StateStringProtoPair{"STOP_REQUESTED",
                             ::boca::ReceiverConnectionState::STOP_REQUESTED},
        StateStringProtoPair{"CONNECTING",
                             ::boca::ReceiverConnectionState::CONNECTING},
        StateStringProtoPair{"CONNECTED",
                             ::boca::ReceiverConnectionState::CONNECTED},
        StateStringProtoPair{"DISCONNECTED",
                             ::boca::ReceiverConnectionState::DISCONNECTED},
        StateStringProtoPair{"ERROR", ::boca::ReceiverConnectionState::ERROR}));

}  // namespace
}  // namespace ash::boca_receiver
