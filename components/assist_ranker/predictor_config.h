// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_H_
#define COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/field_trial_params.h"

namespace assist_ranker {

// TODO(chrome-ranker-team): Implement other logging types.
enum LogType {
  LOG_NONE = 0,
  LOG_UKM = 1,
};

// Empty feature allowlist used for testing.
const base::flat_set<std::string>* GetEmptyAllowlist();

// This struct holds the config options for logging, loading and field trial
// for a predictor.
struct PredictorConfig {
  PredictorConfig(const char* model_name,
                  const char* logging_name,
                  const char* uma_prefix,
                  const LogType log_type,
                  const base::flat_set<std::string>* feature_allowlist,
                  const base::Feature* field_trial,
                  const base::FeatureParam<std::string>* field_trial_url_param,
                  float field_trial_threshold_replacement_param)
      : model_name(model_name),
        logging_name(logging_name),
        uma_prefix(uma_prefix),
        log_type(log_type),
        feature_allowlist(feature_allowlist),
        field_trial(field_trial),
        field_trial_url_param(field_trial_url_param),
        field_trial_threshold_replacement_param(
            field_trial_threshold_replacement_param) {}
  const char* const model_name;
  const char* const logging_name;
  const char* const uma_prefix;
  const LogType log_type;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const base::flat_set<std::string>* feature_allowlist;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const base::Feature* field_trial;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const base::FeatureParam<std::string>*
      field_trial_url_param;
  const float field_trial_threshold_replacement_param;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_H_
