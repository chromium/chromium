// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/hash_util.h"

#include <limits.h>
#include <vector>

#include "base/check_op.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromecast/base/legacy_app_id_mapper.h"
#include "chromecast/base/version.h"

namespace chromecast {

namespace {

const size_t kAppIdV2StrLen = 8;

}  // namespace

uint64_t HashToUInt64(const std::string& value) {
  uint64_t output;
  const std::string sha1hash = base::SHA1HashString(value);
  DCHECK_GE(sha1hash.size(), sizeof(output));
  memcpy(&output, sha1hash.data(), sizeof(output));
  return output;
}

uint32_t HashToUInt32(const std::string& value) {
  uint32_t output;
  const std::string sha1hash = base::SHA1HashString(value);
  DCHECK_GE(sha1hash.size(), sizeof(output));
  memcpy(&output, sha1hash.data(), sizeof(output));
  return output;
}

uint64_t HashGUID64(const std::string& guid) {
  std::string hex;
  base::RemoveChars(guid, "-", &hex);
  uint64_t output;
  DCHECK_EQ(hex.size(), 32u);
  if (base::HexStringToUInt64(hex.substr(0, 16), &output))
    return output;
  NOTREACHED();
}

uint32_t HashAppId32(const std::string& app_id) {
  uint32_t output;
  if (app_id.size() == kAppIdV2StrLen &&
      base::HexStringToUInt(app_id, &output)) {
    return output;
  }

  return MapLegacyAppId(app_id);
}

uint64_t HashCastBuildNumber64(const std::string& build_number) {
  uint64_t return_value = 0;
  std::vector<std::string> tokens(base::SplitString(
      build_number, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
  const size_t tsize = tokens.size();
  if (tsize < 1 || tsize > 4)
    return static_cast<uint64_t>(-1);

  int bits = 64 / tsize;
  // special case for 3-tuple to make hex look nicer.
  if (tsize == 3)
    bits = 16;

  for (size_t i = 0; i < tsize; ++i) {
    // special case for the last token of 3-tuple.
    if (tsize == 3 && i == 2)
      bits = 32;
    return_value <<= bits;
    unsigned value = 0;
    if (!base::StringToUint(tokens[i], &value))
      return static_cast<uint64_t>(-1);
    if (bits != 64)  // avoid overflow
      value &= (static_cast<uint64_t>(1) << bits) - 1;
    return_value |= value;
  }
  return return_value;
}

uint64_t HashSessionId64(const std::string& session_id) {
  return HashGUID64(session_id);
}

uint64_t HashSdkVersion64(const std::string& sdk_version) {
  if (sdk_version.empty())
    return 0;

  // Sdk version is usually in "X.Y.Z(.minor)" format.
  // Following code is a little bit relaxed to accept all sub-fields optional.
  uint64_t return_value = 0;
  std::vector<std::string> tokens(base::SplitString(
      sdk_version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
  for (size_t i = 0; i < 4; ++i) {
    return_value <<= 16;
    if (tokens.size() > i) {
      unsigned value = 0;
      if (base::StringToUint(tokens[i], &value)) {
        return_value |= value & 0xFFFF;
      } else {
        LOG_IF(ERROR, !CAST_IS_DEBUG_BUILD())
            << "Sdk version " << sdk_version << " is not in correct format.";
        return static_cast<uint64_t>(-1);
      }
    }
  }
  return return_value;
}

uint32_t HashSocketId32(const std::string& socket_id) {
  uint32_t output;
  // socket id is usually just numerical.
  if (base::StringToUint(socket_id, &output)) {
    return output;
  } else {  // CastControlSocket and unittests.
    return HashToUInt32(socket_id);
  }
}

uint32_t HashConnectionId32(const std::string& connection_id) {
  return HashToUInt32(connection_id);
}

uint64_t HashAndroidBuildNumber64(const std::string& build_id) {
  if (build_id.length() == 0) {
    return static_cast<uint64_t>(-1);
  }

  uint64_t value = 0;
  for (const char& c : build_id.substr(0, sizeof(value) / sizeof(char))) {
    value <<= CHAR_BIT;
    value += c;
  }
  return value;
}

}  // namespace chromecast
