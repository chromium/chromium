// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_os/guest_os_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace guest_os {
namespace prefs {

// The following prefs (nested under a provided location, e.g. arc.metrics)
// persist engagement time across sessions, to be accumulated and sent to UMA
// once a day.

// Total engagement time of the device.
const char kEngagementTimeTotal[] = ".engagement_time.total";
// Time spent with a matched window in the foreground.
const char kEngagementTimeForeground[] = ".engagement_time.foreground";
// Time spent without a matched window in the foreground but the guest OS
// otherwise running in the background.
const char kEngagementTimeBackground[] = ".engagement_time.background";
// The OS version when engagement prefs were recorded. Old results will be
// discarded if a version change is detected.
const char kEngagementTimeOsVersion[] = ".engagement_time.os_version";
// The day ID (number of days since origin of Time) when engagement time was
// last recorded.
const char kEngagementTimeDayId[] = ".engagement_time.day_id";

void RegisterEngagementProfilePrefs(PrefRegistrySimple* registry,
                                    const std::string& pref_prefix) {
  registry->RegisterTimeDeltaPref(pref_prefix + kEngagementTimeBackground,
                                  base::TimeDelta());
  registry->RegisterIntegerPref(pref_prefix + kEngagementTimeDayId, 0);
  registry->RegisterTimeDeltaPref(pref_prefix + kEngagementTimeForeground,
                                  base::TimeDelta());
  registry->RegisterStringPref(pref_prefix + kEngagementTimeOsVersion, "");
  registry->RegisterTimeDeltaPref(pref_prefix + kEngagementTimeTotal,
                                  base::TimeDelta());
}

}  // namespace prefs
}  // namespace guest_os
