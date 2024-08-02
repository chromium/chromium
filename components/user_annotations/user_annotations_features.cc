// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_features.h"

#include "base/feature_list.h"

namespace user_annotations {

BASE_FEATURE(kUserAnnotations,
             "UserAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUserAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kUserAnnotations);
}

}  // namespace user_annotations
