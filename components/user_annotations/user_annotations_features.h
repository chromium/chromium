// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_FEATURES_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace user_annotations {

BASE_DECLARE_FEATURE(kUserAnnotations);

// Whether the user annotations feature is enabled.
bool IsUserAnnotationsEnabled();

// Whether the form submission for `url` should be added to user annotations.
bool ShouldAddFormSubmissionForURL(const GURL& url);

// Whether the user annotations should be replaced after each form submission.
bool ShouldReplaceAnnotationsAfterEachSubmission();

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_FEATURES_H_
