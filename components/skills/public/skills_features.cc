// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_features.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_prefs.h"

namespace skills {

bool IsSkillsEnabled(const PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    return false;
  }
  return pref_service && pref_service->GetBoolean(prefs::kChromeSkillsEnabled);
}

}  // namespace skills
