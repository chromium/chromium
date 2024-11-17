// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_hats_utils.h"

#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/prefs/pref_service.h"

namespace plus_addresses::hats {

// static
std::map<std::string, std::string> GetPlusAddressHatsData(
    PrefService* pref_service) {
  auto time_pref_to_string = [&](std::string_view pref) {
    const base::Time time = pref_service->GetTime(pref);
    if (time.is_null()) {
      return std::string("-1");
    }
    const base::TimeDelta delta = base::Time::Now() - time;
    return delta.is_positive() ? base::ToString(delta.InSeconds())
                               : std::string("-1");
  };

  return {{hats::kFirstPlusAddressCreationTime,
           time_pref_to_string(prefs::kFirstPlusAddressCreationTime)},
          {hats::kLastPlusAddressFillingTime,
           time_pref_to_string(prefs::kLastPlusAddressFillingTime)}};
}

}  // namespace plus_addresses::hats
