// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/bluetooth_util.h"

#include "base/strings/stringprintf.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace chromecast {
namespace bluetooth {
namespace util {

namespace {

const int kMacAddrStrLen = 17;
const int kUuid16bitLen = 4;
const int kUuidHexNumChars = 32;
const int kUuidNumDashes = 4;

const char kFmtUuid[] =
    "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx"
    "-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx";

const char kFmtUuidNoDashes[] =
    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx";

const char kFmtAddr[] = "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx";

}  // namespace

// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
const bluetooth_v2_shlib::Uuid kUuidBase = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x10, 0x00, 0x80, 0x00, 0x00, 0x80,
                                             0x5F, 0x9B, 0x34, 0xFB}};

std::string AddrToString(const bluetooth_v2_shlib::Addr& addr) {
  return base::StringPrintf(kFmtAddr, addr[5], addr[4], addr[3], addr[2],
                            addr[1], addr[0]);
}

std::string AddrLastByteString(const bluetooth_v2_shlib::Addr& addr) {
  return base::StringPrintf("%02hhx", addr[0]);
}

bool ParseAddr(const std::string& str, bluetooth_v2_shlib::Addr* addr) {
  // sscanf will incorrectly succeed if all characters except the last one are
  // correct.
  if (str.size() != kMacAddrStrLen ||
      !absl::ascii_isxdigit(static_cast<unsigned char>(str.back()))) {
    return false;
  }

  int ret = sscanf(str.c_str(), kFmtAddr, &(*addr)[5], &(*addr)[4], &(*addr)[3],
                   &(*addr)[2], &(*addr)[1], &(*addr)[0]);

  return ret == static_cast<int>(addr->size());
}

std::string UuidToString(const bluetooth_v2_shlib::Uuid& uuid) {
  return base::StringPrintf(kFmtUuid, uuid[0], uuid[1], uuid[2], uuid[3],
                            uuid[4], uuid[5], uuid[6], uuid[7], uuid[8],
                            uuid[9], uuid[10], uuid[11], uuid[12], uuid[13],
                            uuid[14], uuid[15]);
}

bool ParseUuid(const std::string& str, bluetooth_v2_shlib::Uuid* uuid) {
  if (str.empty()) {
    return false;
  }

  for (char c : str) {
    if (c != '-' && !absl::ascii_isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }

  // Check for 16-bit UUID
  if (str.size() == kUuid16bitLen) {
    *uuid = kUuidBase;
    return sscanf(str.c_str(), "%02hhx%02hhx", &(*uuid)[2], &(*uuid)[3]) == 2;
  }

  if (str.size() > kUuidHexNumChars &&
      str.size() != kUuidHexNumChars + kUuidNumDashes) {
    return false;
  }

  std::string no_dashes = str;
  no_dashes.erase(std::remove(no_dashes.begin(), no_dashes.end(), '-'),
                  no_dashes.end());

  if (no_dashes.size() != kUuidHexNumChars) {
    return false;
  }

  int ret =
      sscanf(no_dashes.c_str(), kFmtUuidNoDashes, &(*uuid)[0], &(*uuid)[1],
             &(*uuid)[2], &(*uuid)[3], &(*uuid)[4], &(*uuid)[5], &(*uuid)[6],
             &(*uuid)[7], &(*uuid)[8], &(*uuid)[9], &(*uuid)[10], &(*uuid)[11],
             &(*uuid)[12], &(*uuid)[13], &(*uuid)[14], &(*uuid)[15]);

  return ret == static_cast<int>(uuid->size());
}

bluetooth_v2_shlib::Uuid UuidFromInt16(uint16_t uuid) {
  bluetooth_v2_shlib::Uuid ret = kUuidBase;
  ret[2] = (uuid >> 8) & 0xff;
  ret[3] = uuid & 0xff;
  return ret;
}

bluetooth_v2_shlib::Uuid UuidFromInt32(uint32_t uuid) {
  bluetooth_v2_shlib::Uuid ret = kUuidBase;
  ret[0] = (uuid >> 24) & 0xff;
  ret[1] = (uuid >> 16) & 0xff;
  ret[2] = (uuid >> 8) & 0xff;
  ret[3] = uuid & 0xff;
  return ret;
}

}  // namespace util
}  // namespace bluetooth
}  // namespace chromecast
