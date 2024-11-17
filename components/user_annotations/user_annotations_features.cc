// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_features.h"

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"

namespace user_annotations {

bool IsUserAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(autofill_ai::kAutofillAi);
}

std::vector<std::string> GetAllowedHostsForFormsAnnotations() {
  std::string allowed_hosts_string = base::GetFieldTrialParamValueByFeature(
      autofill_ai::kAutofillAi, "allowed_hosts_for_form_submissions");
  return base::SplitString(allowed_hosts_string, ",",
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

bool ShouldExtractAXTreeForFormsAnnotations() {
  return base::GetFieldTrialParamByFeatureAsBool(
      autofill_ai::kAutofillAi, "should_extract_ax_tree_for_forms_annotations",
      false);
}

base::TimeDelta GetFormSubmissionCompletionTimeout() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      autofill_ai::kAutofillAi, "form_submission_completion_timeout",
      base::Seconds(30));
}

}  // namespace user_annotations
