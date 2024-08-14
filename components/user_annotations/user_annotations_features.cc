// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"

namespace user_annotations {

BASE_FEATURE(kUserAnnotations,
             "UserAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUserAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kUserAnnotations);
}

bool ShouldAddFormSubmissionForURL(const GURL& url) {
  std::string allowed_hosts_string = base::GetFieldTrialParamValueByFeature(
      kUserAnnotations, "allowed_hosts_for_form_submissions");
  if (allowed_hosts_string.empty() || allowed_hosts_string == "*") {
    return true;
  }
  std::vector<std::string_view> allowed_hosts = base::SplitStringPiece(
      allowed_hosts_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return base::Contains(allowed_hosts, url.host());
}

bool ShouldReplaceAnnotationsAfterEachSubmission() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kUserAnnotations, "should_replace_annotations_for_form_submissions",
      false);
}

}  // namespace user_annotations
