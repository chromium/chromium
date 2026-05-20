// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_PREFS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace skills::prefs {

// String name of the boolean preference for whether Skills is enabled.
extern const char kChromeSkillsEnabled[];

// Registers the profile-specific preferences for the Skills feature.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace skills::prefs

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_PREFS_H_
