// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_features.h"

#include "base/feature_list.h"
#include "base/strings/string_split.h"

namespace user_annotations {

BASE_FEATURE(kUserAnnotations,
             "UserAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUserAnnotationsObserveFormSubmissions,
             "UserAnnotationsObserveFormSubmissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUserAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kUserAnnotations);
}

bool IsUserAnnotationsObserveFormSubmissionsEnabled() {
  return base::FeatureList::IsEnabled(kUserAnnotationsObserveFormSubmissions);
}

std::vector<std::string> GetAllowedHostsForFormsAnnotations() {
  std::string allowed_hosts_string = base::GetFieldTrialParamValueByFeature(
      kUserAnnotations, "allowed_hosts_for_form_submissions");
  return base::SplitString(allowed_hosts_string, ",",
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

bool ShouldPersistUserAnnotations() {
  return base::GetFieldTrialParamByFeatureAsBool(kUserAnnotations,
                                                 "persist_annotations", false);
}

bool ShouldExtractAXTreeForFormsAnnotations() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kUserAnnotations, "should_extract_ax_tree_for_forms_annotations", false);
}

base::TimeDelta GetFormSubmissionCompletionTimeout() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kUserAnnotations, "form_submission_completion_timeout",
      base::Seconds(30));
}

}  // namespace user_annotations
