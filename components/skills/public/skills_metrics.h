// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_

namespace skills {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SkillsActions)
enum class SkillsActions {
  kSavedSkill = 0,
  kClickedTryItNow = 1,
  kUsed1stPartySkill = 2,
  kUsedUserCreatedSkill = 3,
  kOpenedCreationDialog = 4,
  kClickedCancelInCreationDialog = 5,
  kClickedRefineInCreationDialog = 6,
  kOpenedManageSkillsPage = 7,
  kMaxValue = kOpenedManageSkillsPage,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsActions)

// Records a discrete user action (e.g., clicking a button).
void RecordSkillsAction(SkillsActions action);

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
