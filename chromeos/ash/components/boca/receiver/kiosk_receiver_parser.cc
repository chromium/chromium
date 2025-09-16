// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"

#include <string>
#include <string_view>
#include <vector>

#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/base_requests.h"

namespace ash::boca_receiver {
namespace {

struct StateStringProtoPair {
  std::string_view state_string;
  ::boca::ReceiverConnectionState state_proto;
};

constexpr std::string_view kReceiverStateUknown = "RECEIVER_STATE_UNKNOWN";
static constexpr StateStringProtoPair kStateStringProtoPairs[] = {
    {kReceiverStateUknown,
     ::boca::ReceiverConnectionState::RECEIVER_CONNECTION_STATE_UNKNOWN},
    {"START_REQUESTED", ::boca::ReceiverConnectionState::START_REQUESTED},
    {"STOP_REQUESTED", ::boca::ReceiverConnectionState::STOP_REQUESTED},
    {"CONNECTING", ::boca::ReceiverConnectionState::CONNECTING},
    {"CONNECTED", ::boca::ReceiverConnectionState::CONNECTED},
    {"DISCONNECTED", ::boca::ReceiverConnectionState::DISCONNECTED},
    {"ERROR", ::boca::ReceiverConnectionState::ERROR},
};

}  // namespace

::boca::ReceiverConnectionState ReceiverConnectionStateProtoFromJson(
    const std::string& state) {
  for (const StateStringProtoPair& pair : kStateStringProtoPairs) {
    if (pair.state_string == state) {
      return pair.state_proto;
    }
  }
  return ::boca::ReceiverConnectionState::RECEIVER_CONNECTION_STATE_UNKNOWN;
}

std::string ReceiverConnectionStateStringFromProto(
    ::boca::ReceiverConnectionState state) {
  for (const StateStringProtoPair& pair : kStateStringProtoPairs) {
    if (pair.state_proto == state) {
      return std::string(pair.state_string);
    }
  }
  return std::string(kReceiverStateUknown);
}

}  // namespace ash::boca_receiver
