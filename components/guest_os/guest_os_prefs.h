// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_OS_GUEST_OS_PREFS_H_
#define COMPONENTS_GUEST_OS_GUEST_OS_PREFS_H_

#include <string>

class PrefRegistrySimple;

namespace guest_os {
namespace prefs {

extern const char kEngagementTimeTotal[];
extern const char kEngagementTimeForeground[];
extern const char kEngagementTimeBackground[];
extern const char kEngagementTimeOsVersion[];
extern const char kEngagementTimeDayId[];

// Registers prefs used by GuestOsEngagementMetrics.
void RegisterEngagementProfilePrefs(PrefRegistrySimple* registry,
                                    const std::string& pref_prefix);

}  // namespace prefs
}  // namespace guest_os

#endif  // COMPONENTS_GUEST_OS_GUEST_OS_PREFS_H_
