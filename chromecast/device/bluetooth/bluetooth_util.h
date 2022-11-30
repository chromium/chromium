// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_BLUETOOTH_UTIL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_BLUETOOTH_UTIL_H_

#include <string>

#include "chromecast/public/bluetooth/bluetooth_types.h"

namespace chromecast {
namespace bluetooth {
namespace util {

// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
// BASE_UUID 00000000-0000-1000-8000-00805F9B34FB
extern const bluetooth_v2_shlib::Uuid kUuidBase;

// Format |addr| into the canonical text representation of a 48 bit mac address
// (1a:2b:3c:4e:5f:60). Hex digits are lower case.
std::string AddrToString(const bluetooth_v2_shlib::Addr& addr);

// Get the last byte of |addr| as a hex string. This is used for logging since
// full address is PII.
std::string AddrLastByteString(const bluetooth_v2_shlib::Addr& addr);

// Parse |str| as the canonical text representation of a 48 bit mac
// address (1a:2b:3c:4e:5f:60). Hex digits may be either upper or lower case.
//
// Returns true iff |str| is a valid mac address.
bool ParseAddr(const std::string& str, bluetooth_v2_shlib::Addr* addr);

// Format |uuid| as the canonical big endian text format (with lowercase hex
// digits).
// 123e4567-e89b-12d3-a456-426655440000
std::string UuidToString(const bluetooth_v2_shlib::Uuid& uuid);

// Parses UUIDs of the following formats:
// Canonical big endian: 123e4567-e89b-12d3-a456-426655440000
// Bluetooth SIG 16-bit UUID: FEA0
// Big endian no dashes: 123e4567e89b12d3a456426655440000
//
// Hex digits may be either upper or lower case.
// Returns true iff |str| is a UUID.
bool ParseUuid(const std::string& str, bluetooth_v2_shlib::Uuid* uuid);

// Return full UUID object corresponding to 16 bit uuid.
bluetooth_v2_shlib::Uuid UuidFromInt16(uint16_t uuid);

// Return full UUID object corresponding to 32 bit uuid.
bluetooth_v2_shlib::Uuid UuidFromInt32(uint32_t uuid);

}  // namespace util
}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_BLUETOOTH_UTIL_H_
