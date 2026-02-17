// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace skills {

void RecordSkillsAction(SkillsActions action) {
  base::UmaHistogramEnumeration("Skills.Actions", action);
}

}  // namespace skills
