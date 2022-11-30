// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_PROTO_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_PROTO_TEST_UTIL_H_

#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash {

namespace tether {

namespace proto_test_util {

// Pass these constants to CreateTestDeviceStatus() to create a proto object
// which does not have the associated field set.
const char kDoNotSetStringField[] = "doNotSetField";
const int kDoNotSetIntField = -100;

}  // namespace proto_test_util

// Creates a DeviceStatus object using the parameters provided. If
// |kDoNotSetStringField| or |kDoNotSetIntField| are passed, these fields will
// not be set in the output.
DeviceStatus CreateTestDeviceStatus(const std::string& cell_provider_name,
                                    int battery_percentage,
                                    int connection_strength);

// Creates a DeviceStatus object using fake field values.
DeviceStatus CreateDeviceStatusWithFakeFields();

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_PROTO_TEST_UTIL_H_
