// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace skills {

void RecordSkillsDialogAction(SkillsDialogAction action, bool is_edit_mode) {
  if (is_edit_mode) {
    base::UmaHistogramEnumeration("Skills.Dialog.Edit.Action", action);
  } else {
    base::UmaHistogramEnumeration("Skills.Dialog.Creation.Action", action);
  }
}

void RecordSkillsInvokeAction(SkillsInvokeAction action) {
  base::UmaHistogramEnumeration("Skills.Invoke.Action", action);
}

void RecordSkillsInvokeResult(SkillsInvokeResult result) {
  base::UmaHistogramEnumeration("Skills.Invoke.Result", result);
}

void RecordSkillsSaveResult(SkillsSaveResult result) {
  base::UmaHistogramEnumeration("Skills.Save.Result", result);
}

void RecordSkillsRefineResult(SkillsRefineResult result) {
  base::UmaHistogramEnumeration("Skills.Refine.Result", result);
}

void RecordUserSkillCount(size_t skill_count) {
  base::UmaHistogramCounts1000("Skills.UserSkills.Count",
                               base::checked_cast<int>(skill_count));
}

void RecordSkillsFetchResult(SkillsFetchResult result) {
  base::UmaHistogramEnumeration("Skills.Downloader.FirstParty.FetchResult",
                                result);
}

void RecordSkillsHttpCode(int http_code) {
  base::UmaHistogramSparse("Skills.Downloader.FirstParty.HttpResponseCode",
                           http_code);
}

void RecordSkillsDownloadRequestStatus(SkillsDownloadRequestStatus status) {
  base::UmaHistogramEnumeration(
      "Skills.Management.FirstParty.DownloadRequestStatus", status);
}

void RecordSkillsManagementError(SkillsManagementError error) {
  base::UmaHistogramEnumeration("Skills.Management.Error", error);
}

}  // namespace skills
