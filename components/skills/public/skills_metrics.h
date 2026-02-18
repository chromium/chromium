// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_

namespace skills {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(SkillsDialogAction)
enum class SkillsDialogAction {
  kOpened = 0,
  kSaved = 1,
  kCancelled = 2,
  kRefined = 3,
  kMaxValue = kRefined,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsDialogAction)

// LINT.IfChange(SkillsInvokeAction)
enum class SkillsInvokeAction {
  kFirstParty = 0,
  kUserCreated = 1,
  kDerivedFromFirstParty = 2,
  kMaxValue = kDerivedFromFirstParty,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsInvokeAction)

// TODO(crbug.com/477385216): Update to use an enum for creation mode.
// Records user interactions within the Skills Creation or Edit dialogs
void RecordSkillsDialogAction(SkillsDialogAction action, bool is_edit_mode);

// Records the execution of a skill and its type.
void RecordSkillsInvokeAction(SkillsInvokeAction action);

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
