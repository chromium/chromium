// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_EXPERIMENT_GROUP_IDS_H_
#define COMPONENTS_VARIATIONS_EXPERIMENT_GROUP_IDS_H_

#include "base/component_export.h"
#include "components/variations/proto/study.pb.h"

namespace variations {

// Returns true if the experiment group sets google_web_experiment_id or
// google_web_trigger_experiment_id.
COMPONENT_EXPORT(VARIATIONS)
bool HasGoogleWebExperimentId(const Study::Experiment& experiment);

// Returns true if any experiment group in `study` sets google_web_experiment_id
// or google_web_trigger_experiment_id.
COMPONENT_EXPORT(VARIATIONS)
bool HasGoogleWebExperimentId(const Study& study);

// Returns true if the experiment group sets any of the following fields:
// * google_web_experiment_id
// * google_web_trigger_experiment_id
// * google_app_experiment_id
COMPONENT_EXPORT(VARIATIONS)
bool HasExperimentId(const Study::Experiment& experiment);

// Returns true if the experiment group has probability_weight > 0 and has
// any experiment ID (see HasExperimentId()).
COMPONENT_EXPORT(VARIATIONS)
bool IsWeightedGroupWithExperimentId(const Study::Experiment& experiment);

// Returns true if any experiment group in `study` has probability_weight > 0
// and sets google_web_experiment_id or google_web_trigger_experiment_id.
COMPONENT_EXPORT(VARIATIONS)
bool HasWeightedGroupWithGoogleWebExperimentId(const Study& study);

// Returns true if any experiment group in `study` has probability_weight > 0
// and has any experiment ID (see HasExperimentId()).
COMPONENT_EXPORT(VARIATIONS)
bool HasWeightedGroupWithExperimentId(const Study& study);

// Returns true if the study consumes entropy. A study consumes entropy if it
// has permanent consistency and has a weighted group with an experiment ID.
COMPONENT_EXPORT(VARIATIONS)
bool ConsumesEntropy(const Study& study);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_EXPERIMENT_GROUP_IDS_H_
