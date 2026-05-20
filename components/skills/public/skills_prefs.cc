// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace skills::prefs {

const char kChromeSkillsEnabled[] = "skills.enabled";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kChromeSkillsEnabled, true);
}

}  // namespace skills::prefs
