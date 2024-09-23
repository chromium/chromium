// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/metrics.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"

namespace proximity_auth {
namespace metrics {

namespace {

// Converts the 4-byte prefix of an MD5 hash into a int32_t value.
int32_t DigestToInt32(const base::MD5Digest& digest) {
  return static_cast<int32_t>(
      base::numerics::U32FromLittleEndian(base::span(digest.a).first<4u>()));
}

// Returns a hash of the given |name|, encoded as a 32-bit signed integer.
int32_t HashDeviceModelName(const std::string& name) {
  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(name), &digest);
  return DigestToInt32(digest);
}

}  // namespace

const char kUnknownDeviceModel[] = "Unknown";
const int kUnknownProximityValue = 127;

void RecordAuthProximityRollingRssi(int rolling_rssi) {
  if (rolling_rssi != kUnknownProximityValue)
    rolling_rssi = std::clamp(rolling_rssi, -100, 50);

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
