// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_HASH_UTIL_H_
#define CHROMECAST_BASE_HASH_UTIL_H_

#include <stdint.h>

#include <string>

namespace chromecast {

// These are utils to hash strings to uma proto integers.

// Common utils to hash strings.
uint64_t HashToUInt64(const std::string& value);
uint32_t HashToUInt32(const std::string& value);
uint64_t HashGUID64(const std::string& guid);

// Common utils to hash cast-related ids.
uint32_t HashAppId32(const std::string& app_id);
uint64_t HashCastBuildNumber64(const std::string& build_number);
uint64_t HashSessionId64(const std::string& session_id);
uint64_t HashSdkVersion64(const std::string& sdk_version);
uint32_t HashSocketId32(const std::string& socket_id);
uint32_t HashConnectionId32(const std::string& connection_id);

// Encodes the first 8 characters build_id into a uint64
uint64_t HashAndroidBuildNumber64(const std::string& build_id);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_HASH_UTIL_H_
