// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_HASH_UTIL_H_
#define CHROMECAST_BASE_HASH_UTIL_H_

#include <stdint.h>

#include <string_view>

namespace chromecast {

// These are utils to hash strings to uma proto integers.

// Common utils to hash strings.
uint64_t HashToUInt64(std::string_view value);
uint32_t HashToUInt32(std::string_view value);
uint64_t HashGUID64(std::string_view guid);

// Common utils to hash cast-related ids.
uint32_t HashAppId32(std::string_view app_id);
uint64_t HashCastBuildNumber64(std::string_view build_number);
uint64_t HashSessionId64(std::string_view session_id);
uint64_t HashSdkVersion64(std::string_view sdk_version);
uint32_t HashSocketId32(std::string_view socket_id);
uint32_t HashConnectionId32(std::string_view connection_id);

// Encodes the first 8 characters build_id into a uint64
uint64_t HashAndroidBuildNumber64(std::string_view build_id);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_HASH_UTIL_H_
