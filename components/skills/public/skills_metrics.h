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

// LINT.IfChange(SkillsFetchResult)
enum class SkillsFetchResult {
  kSuccess = 0,
  kEmptyResponseBody = 1,
  kEmptyResponseHeader = 2,
  kProtoParseFailure = 3,
  kNetworkError = 4,
  kMaxValue = kNetworkError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsFetchResult)

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

// Records the result of a first-party skill list download attempt from static
// content server link.
void RecordSkillsFetchResult(SkillsFetchResult result);

// Records the HTTP response code received when downloading skills.
void RecordSkillsHttpCode(int http_code);

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
