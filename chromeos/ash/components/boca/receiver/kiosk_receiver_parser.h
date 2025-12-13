// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_KIOSK_RECEIVER_PARSER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_KIOSK_RECEIVER_PARSER_H_

#include <string>

#include "chromeos/ash/components/boca/proto/receiver.pb.h"

namespace ash::boca_receiver {

::boca::ReceiverConnectionState ReceiverConnectionStateProtoFromJson(
    const std::string& state);

std::string ReceiverConnectionStateStringFromProto(
    ::boca::ReceiverConnectionState state);

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_KIOSK_RECEIVER_PARSER_H_
