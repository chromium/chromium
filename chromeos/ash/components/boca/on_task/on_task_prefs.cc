// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_prefs.h"

#include "base/check.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash::boca {

// A list pref used to track disabled extensions for OnTask.
const char kDisabledOnTaskExtensions[] = "boca.disabled_on_task_extensions";

void RegisterOnTaskProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  CHECK(registry);
  registry->RegisterListPref(kDisabledOnTaskExtensions);
}

}  // namespace ash::boca
