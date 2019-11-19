// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/metrics.h"

#include <stdint.h>

#include <algorithm>

#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"

namespace proximity_auth {
namespace metrics {

namespace {

// Converts the 4-byte prefix of an MD5 hash into a int32_t value.
int32_t DigestToInt32(const base::MD5Digest& digest) {
  // First, copy to a uint32_t, since byte swapping and endianness conversions
  // expect unsigned integers.
  uint32_t unsigned_value;
  DCHECK_GE(base::size(digest.a), sizeof(unsigned_value));
  memcpy(&unsigned_value, digest.a, sizeof(unsigned_value));
  unsigned_value = base::ByteSwap(base::HostToNet32(unsigned_value));

  // Then copy the resulting bit pattern to an int32_t to match the datatype
  // that histograms expect.
  int32_t value;
  memcpy(&value, &unsigned_value, sizeof(value));
  return value;
}

// Returns a hash of the given |name|, encoded as a 32-bit signed integer.
int32_t HashDeviceModelName(const std::string& name) {
  base::MD5Digest digest;
  base::MD5Sum(name.c_str(), name.size(), &digest);
  return DigestToInt32(digest);
}

}  // namespace

const char kUnknownDeviceModel[] = "Unknown";
const int kUnknownProximityValue = 127;

void RecordAuthProximityRollingRssi(int rolling_rssi) {
  if (rolling_rssi != kUnknownProximityValue)
    rolling_rssi = base::ClampToRange(rolling_rssi, -100, 50);

  base::UmaHistogramSparse("EasyUnlock.AuthProximity.RollingRssi",
                           rolling_rssi);
}

void RecordAuthProximityRemoteDeviceModelHash(const std::string& device_model) {
  base::UmaHistogramSparse("EasyUnlock.AuthProximity.RemoteDeviceModelHash",
                           HashDeviceModelName(device_model));
}

void RecordRemoteSecuritySettingsState(RemoteSecuritySettingsState state) {
  DCHECK(state < RemoteSecuritySettingsState::COUNT);
  UMA_HISTOGRAM_ENUMERATION(
      "EasyUnlock.RemoteLockScreenState", static_cast<int>(state),
      static_cast<int>(RemoteSecuritySettingsState::COUNT));
}

}  // namespace metrics
}  // namespace proximity_auth
