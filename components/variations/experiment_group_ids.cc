// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/experiment_group_ids.h"

#include <algorithm>

#include "components/variations/proto/study.pb.h"

namespace variations {

bool HasGoogleWebExperimentId(const Study::Experiment& experiment) {
  return experiment.has_google_web_experiment_id() ||
         experiment.has_google_web_trigger_experiment_id();
}

bool HasGoogleWebExperimentId(const Study& study) {
  return std::ranges::any_of(study.experiment(),
                             [](const Study::Experiment& experiment) {
                               return HasGoogleWebExperimentId(experiment);
                             });
}

bool HasExperimentId(const Study::Experiment& experiment) {
  return HasGoogleWebExperimentId(experiment) ||
         experiment.has_google_app_experiment_id();
}

bool IsWeightedGroupWithExperimentId(const Study::Experiment& experiment) {
  return experiment.probability_weight() > 0 && HasExperimentId(experiment);
}

bool HasWeightedGroupWithGoogleWebExperimentId(const Study& study) {
  return std::ranges::any_of(study.experiment(),
                             [](const Study::Experiment& experiment) {
                               return experiment.probability_weight() > 0 &&
                                      HasGoogleWebExperimentId(experiment);
                             });
}

bool HasWeightedGroupWithExperimentId(const Study& study) {
  return std::ranges::any_of(
      study.experiment(), [](const Study::Experiment& experiment) {
        return IsWeightedGroupWithExperimentId(experiment);
      });
}

}  // namespace variations
