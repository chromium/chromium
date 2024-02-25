// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_BASE_PREDICTOR_H_
#define COMPONENTS_ASSIST_RANKER_BASE_PREDICTOR_H_

#include <memory>
#include <string>

#include "components/assist_ranker/predictor_config.h"
#include "components/assist_ranker/ranker_model_loader.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace ukm {
class UkmEntryBuilder;
}

namespace assist_ranker {

// Value to use for when no prediction threshold replacement should be applied.
// See |GetPredictThresholdReplacement| method.
const float kNoPredictThresholdReplacement = 0.0;

class Feature;
class RankerExample;
class RankerModel;

// Predictors are objects that provide an interface for prediction, as well as
// encapsulate the logic for loading the model and logging. Sub-classes of
// BasePredictor implement an interface that depends on the nature of the
// supported model. Subclasses of BasePredictor will also need to implement an
// Initialize method that will be called once the model is available, and a
// static validation function with the following signature:
//
// static RankerModelStatus ValidateModel(const RankerModel& model);
class BasePredictor {
 public:
  BasePredictor(const PredictorConfig& config);

  BasePredictor(const BasePredictor&) = delete;
  BasePredictor& operator=(const BasePredictor&) = delete;

  virtual ~BasePredictor();

  // Returns true if the predictor is ready to make predictions.
  bool IsReady();
  // Returns true if the base::Feature associated with this model is enabled.
  bool is_query_enabled() const { return is_query_enabled_; }

  // Logs the features of |example| to UKM using the given source_id.
  void LogExampleToUkm(const RankerExample& example, ukm::SourceId source_id);

  // Returns the model URL.
  GURL GetModelUrl() const;
  // Returns the threshold to use for prediction, or
  // kNoPredictThresholdReplacement to leave it unchanged.
  float GetPredictThresholdReplacement() const;
  // Returns the model name.
  std::string GetModelName() const;

 protected:
  // Preprocessing applied to an example before prediction. The original
  // RankerExample is not modified, so it is safe to use it later for logging.
  RankerExample PreprocessExample(const RankerExample& example);

  // Called when the RankerModelLoader has finished loading the model. Returns
  // true only if the model was succesfully loaded and is ready to predict.
  virtual bool Initialize() = 0;

  // Loads a model and make it available for prediction.
  void LoadModel(std::unique_ptr<RankerModelLoader> model_loader);
  // Called once the model loader as succesfully loaded the model.
  void OnModelAvailable(std::unique_ptr<RankerModel> model);
  std::unique_ptr<RankerModelLoader> model_loader_;
  // The model used for prediction.
  std::unique_ptr<RankerModel> ranker_model_;

 private:
  void LogFeatureToUkm(const std::string& feature_name,
                       const Feature& feature,
                       ukm::UkmEntryBuilder* ukm_builder);

  bool is_ready_ = false;
  bool is_query_enabled_ = false;
  PredictorConfig config_;
};

}  // namespace assist_ranker
#endif  // COMPONENTS_ASSIST_RANKER_BASE_PREDICTOR_H_
