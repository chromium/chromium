// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_ENUM_UTIL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_ENUM_UTIL_H_

#include <ostream>

#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"

namespace ash {

namespace device_sync {

std::ostream& operator<<(std::ostream& stream,
                         const cryptauth::SoftwareFeature& software_fature);

// Converts the string representation of a SoftwareFeature to its associated
// Proto enum value. Some Proto messages are sent to Apiary endpoints, which
// translate Proto enums to strings instead of leaving them as enums. Thus, when
// communicating with those endpoints, the proto values should be converted from
// enums to strings before sending them.
cryptauth::SoftwareFeature SoftwareFeatureStringToEnum(
    const std::string& software_feature_as_string);

// Converts a Proto enum SoftwareFeature to its associated string
// representation. Some Proto messages are sent to Apiary endpoints, which
// translate Proto enums to strings instead of leaving them as enums. Thus, when
// communicating with those endpoints, the proto values should be converted from
// strings to enums after receiving them.
std::string SoftwareFeatureEnumToString(
    cryptauth::SoftwareFeature software_feature);

// Converts a Proto enum SoftwareFeature to its ALL_CAPS string
// representation. Used to specify software features for the FindEligibleDevices
// request's |callback_bluetooth_address| field.
std::string SoftwareFeatureEnumToStringAllCaps(
    cryptauth::SoftwareFeature software_feature);

std::ostream& operator<<(std::ostream& stream,
                         const cryptauth::DeviceType& device_type);

// Converts the string representation of a DeviceType to its associated
// Proto enum value. Some Proto messages are sent to Apiary endpoints, which
// translate Proto enums to strings instead of leaving them as enums. Thus, when
// communicating with those endpoints, the proto values should be converted from
// enums to strings before sending them.
cryptauth::DeviceType DeviceTypeStringToEnum(
    const std::string& device_type_as_string);

// Converts a Proto enum DeviceType to its associated string
// representation. Some Proto messages are sent to Apiary endpoints, which
// translate Proto enums to strings instead of leaving them as enums. Thus, when
// communicating with those endpoints, the proto values should be converted from
// strings to enums after receiving them.
std::string DeviceTypeEnumToString(cryptauth::DeviceType device_type);

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_ENUM_UTIL_H_
