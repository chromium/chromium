// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_PREFS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ash::boca {

// A list pref used to track disabled extensions for OnTask.
extern const char kDisabledOnTaskExtensions[];

void RegisterOnTaskProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_PREFS_H_
