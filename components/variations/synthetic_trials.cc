// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trials.h"

namespace variations {

SyntheticTrialGroup::SyntheticTrialGroup(
    uint32_t trial,
    uint32_t group,
    SyntheticTrialAnnotationMode annotation_mode)
    : annotation_mode(annotation_mode) {
  id.name = trial;
  id.group = group;
}

SyntheticTrialGroup::~SyntheticTrialGroup() {}

}  // namespace variations
