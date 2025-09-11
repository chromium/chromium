// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"

#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/base_requests.h"

namespace ash::boca_receiver {
::boca::ReceiverConnectionState ReceiverConnectionStateProtoFromJson(
    const std::string& state) {
  if (state == "RECEIVER_STATE_UNKNOWN") {
    return ::boca::ReceiverConnectionState::RECEIVER_CONNECTION_STATE_UNKNOWN;
  }
  if (state == "START_REQUESTED") {
    return ::boca::ReceiverConnectionState::START_REQUESTED;
  }
  if (state == "STOP_REQUESTED") {
    return ::boca::ReceiverConnectionState::STOP_REQUESTED;
  }
  if (state == "CONNECTING") {
    return ::boca::ReceiverConnectionState::CONNECTING;
  }
  if (state == "CONNECTED") {
    return ::boca::ReceiverConnectionState::CONNECTED;
  }
  if (state == "DISCONNECTED") {
    return ::boca::ReceiverConnectionState::DISCONNECTED;
  }
  if (state == "ERROR") {
    return ::boca::ReceiverConnectionState::ERROR;
  }
  return ::boca::ReceiverConnectionState::RECEIVER_CONNECTION_STATE_UNKNOWN;
}
}  // namespace ash::boca_receiver
