// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_FEATURES_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_FEATURES_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace user_annotations {

// Whether the user annotations feature is enabled.
bool IsUserAnnotationsEnabled();

// Returns the set of hosts that are allowed for forms annotations.
std::vector<std::string> GetAllowedHostsForFormsAnnotations();

bool ShouldExtractAXTreeForFormsAnnotations();

// Returns the completion timeout for the entirety of forms submission stages.
base::TimeDelta GetFormSubmissionCompletionTimeout();

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_FEATURES_H_
