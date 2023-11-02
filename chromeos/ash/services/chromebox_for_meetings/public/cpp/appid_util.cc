// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/appid_util.h"

#include "base/containers/contains.h"

namespace ash {
namespace cfm {

namespace {
// List of allowed internal App IDs for CfM.
constexpr char const* kInternalHotrodAppIds[] = {
    "moklfjoegmpoolceggbebbmgbddlhdgp",  // Stable
    "ldmpofkllgeicjiihkimgeccbhghhmfj",  // Beta
    "denipklgekfpcdmbahmbpnmokgajnhma",  // Alpha
    "kjfhgcncjdebkoofmbjoiemiboifnpbo",  // Dev
    // Keep in sync with app_info.ts (go/googlehotrodappids).
};

// List of allowed external App IDs for CfM.
constexpr char const* kExternalHotrodAppIds[] = {
    "ikfcpmgefdpheiiomgmhlmmkihchmdlj",  // Stable
    "jlgegmdnodfhciolbdjciihnlaljdbjo",  // Beta
    "lkbhffjfgpmpeppncnimiiikojibkhnm",  // Alpha
    "acdafoiapclbpdkhnighhilgampkglpc",  // Dev
    "hkamnlhnogggfddmjomgbdokdkgfelgg",  // TestGaia
    // Keep in sync with app_info.ts (go/hotrodappids).
};

// Returns true if the ID provided matches a valid internal hotrod appid.
bool IsInternalHotrodAppId(const std::string& app_id) {
  return base::Contains(kInternalHotrodAppIds, app_id);
}

// Returns true if the ID provided matches a valid external hotrod appid.
bool IsExternalHotrodAppId(const std::string& app_id) {
  return base::Contains(kExternalHotrodAppIds, app_id);
}

}  // namespace

bool IsChromeboxForMeetingsAppId(const std::string& app_id) {
  return IsExternalHotrodAppId(app_id) || IsInternalHotrodAppId(app_id);
}

}  // namespace cfm
}  // namespace ash
