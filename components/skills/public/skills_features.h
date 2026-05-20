// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_FEATURES_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_FEATURES_H_

class PrefService;

namespace skills {

// Returns true if the Skills in Chrome feature is enabled by both the
// base::Feature flag and the user-visible toggle preference.
// Safe to call with a null `pref_service` (returns false).
bool IsSkillsEnabled(const PrefService* pref_service);

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_FEATURES_H_
