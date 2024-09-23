// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/appid_util.h"

#include "base/containers/contains.h"

namespace chromeos {
namespace cfm {

namespace {
// List of allowed internal App IDs for CfM.
constexpr char const* kInternalHotrodHashedAppIds[] = {
    "4F25792AF1AA7483936DE29C07806F203C7170A0",  // Stable
    "BD8781D757D830FC2E85470A1B6E8A718B7EE0D9",  // Beta
    "4AC2B6C63C6480D150DFDA13E4A5956EB1D0DDBB",  // Alpha
    "81986D4F846CEDDDB962643FA501D1780DD441BB",  // Dev
    // Keep in sync with app_info.ts (go/googlehotrodappids).
};

// List of allowed external App IDs for CfM.
constexpr char const* kExternalHotrodHashedAppIds[] = {
    "E703483CEF33DEC18B4B6DD84B5C776FB9182BDB",  // Stable
    "A3BC37E2148AC4E99BE4B16AF9D42DD1E592BBBE",  // Beta
    "1C93BD3CF875F4A73C0B2A163BB8FBDA8B8B3D80",  // Alpha
    "307E96539209F95A1A8740C713E6998A73657D96",  // Dev
    "A9A9FC0228ADF541F0334F22BEFB8F9C245B21D7",  // TestGaia
    // Keep in sync with app_info.ts (go/hotrodappids).
};

// Returns true if the ID provided matches a valid internal hotrod appid.
bool IsInternalHotrodHashedAppId(const std::string& app_id) {
  return base::Contains(kInternalHotrodHashedAppIds, app_id);
}

// Returns true if the ID provided matches a valid external hotrod appid.
bool IsExternalHotrodHashedAppId(const std::string& app_id) {
  return base::Contains(kExternalHotrodHashedAppIds, app_id);
}

}  // namespace

bool IsChromeboxForMeetingsHashedAppId(const std::string& app_id) {
  return IsExternalHotrodHashedAppId(app_id) ||
         IsInternalHotrodHashedAppId(app_id);
}

}  // namespace cfm
}  // namespace chromeos
