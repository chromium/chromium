// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/base_predictor.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_example_util.h"
#include "components/assist_ranker/ranker_model.h"
#include "url/gurl.h"

namespace assist_ranker {

BasePredictor::BasePredictor(const PredictorConfig& config) : config_(config) {
  // TODO(chrome-ranker-team): validate config.
  if (config_.field_trial) {
    is_query_enabled_ = base::FeatureList::IsEnabled(*config_.field_trial);
  } else {
    DVLOG(0) << "No field trial specified";
  }
}

BasePredictor::~BasePredictor() = default;

void BasePredictor::LoadModel(std::unique_ptr<RankerModelLoader> model_loader) {
  if (!is_query_enabled_)
    return;

  if (model_loader_) {
    DVLOG(0) << "This predictor already has a model loader.";
    return;
  }
  // Take ownership of the model loader.
  model_loader_ = std::move(model_loader);
  // Kick off the initial model load.
  model_loader_->NotifyOfRankerActivity();
}

void BasePredictor::OnModelAvailable(
    std::unique_ptr<assist_ranker::RankerModel> model) {
  ranker_model_ = std::move(model);
  is_ready_ = Initialize();
}

bool BasePredictor::IsReady() {
  if (!is_ready_ && model_loader_)
    model_loader_->NotifyOfRankerActivity();

  return is_ready_;
}

std::string BasePredictor::GetModelName() const {
  return config_.model_name;
}

GURL BasePredictor::GetModelUrl() const {
  if (!config_.field_trial_url_param) {
    DVLOG(1) << "No URL specified.";
    return GURL();
  }

  return GURL(config_.field_trial_url_param->Get());
}

float BasePredictor::GetPredictThresholdReplacement() const {
  return config_.field_trial_threshold_replacement_param;
}

RankerExample BasePredictor::PreprocessExample(const RankerExample& example) {
  if (ranker_model_->proto().has_metadata() &&
      ranker_model_->proto().metadata().input_features_names_are_hex_hashes()) {
    return HashExampleFeatureNames(example);
  }
  return example;
}

}  // namespace assist_ranker
