// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace performance_manager::user_tuning::prefs {

extern const char kHighEfficiencyModeEnabled[];

extern const char kBatterySaverModeEnabled[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace performance_manager::user_tuning::prefs

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_
