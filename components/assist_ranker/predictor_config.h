// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_H_
#define COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"

namespace assist_ranker {

// This struct holds the config options for loading and field trial for a
// predictor.
struct PredictorConfig {
  PredictorConfig(const char* model_name,
                  const char* uma_prefix,
                  const base::Feature* field_trial,
                  const base::FeatureParam<std::string>* field_trial_url_param,
                  float field_trial_threshold_replacement_param)
      : model_name(model_name),
        uma_prefix(uma_prefix),
        field_trial(field_trial),
        field_trial_url_param(field_trial_url_param),
        field_trial_threshold_replacement_param(
            field_trial_threshold_replacement_param) {}
  const char* const model_name;
  const char* const uma_prefix;
  raw_ptr<const base::Feature> field_trial;
  raw_ptr<const base::FeatureParam<std::string>> field_trial_url_param;
  const float field_trial_threshold_replacement_param;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_H_
