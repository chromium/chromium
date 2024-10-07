// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/binary_classifier_predictor.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/assist_ranker/generic_logistic_regression_inference.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_model.h"
#include "components/assist_ranker/ranker_model_loader_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace assist_ranker {

BinaryClassifierPredictor::BinaryClassifierPredictor(
    const PredictorConfig& config)
    : BasePredictor(config) {}
BinaryClassifierPredictor::~BinaryClassifierPredictor() = default;

// static
std::unique_ptr<BinaryClassifierPredictor> BinaryClassifierPredictor::Create(
    const PredictorConfig& config,
    const base::FilePath& model_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  std::unique_ptr<BinaryClassifierPredictor> predictor(
      new BinaryClassifierPredictor(config));
  if (!predictor->is_query_enabled()) {
    DVLOG(1) << "Query disabled, bypassing model loading.";
    return predictor;
  }
  const GURL& model_url = predictor->GetModelUrl();
  DVLOG(1) << "Creating predictor instance for " << predictor->GetModelName();
  DVLOG(1) << "Model URL: " << model_url;
  DVLOG(1) << "Using predict threshold replacement: "
           << predictor->GetPredictThresholdReplacement();
  auto model_loader = std::make_unique<RankerModelLoaderImpl>(
      base::BindRepeating(&BinaryClassifierPredictor::ValidateModel),
      base::BindRepeating(&BinaryClassifierPredictor::OnModelAvailable,
                          base::Unretained(predictor.get())),
      url_loader_factory, model_path, model_url, config.uma_prefix);
  predictor->LoadModel(std::move(model_loader));
  return predictor;
}

bool BinaryClassifierPredictor::Predict(const RankerExample& example,
                                        bool* prediction) {
  if (!IsReady()) {
    DVLOG(1) << "Predictor " << GetModelName() << " not ready for prediction.";
    return false;
  }

  float predict_threshold_replacement = GetPredictThresholdReplacement();
  if (predict_threshold_replacement != kNoPredictThresholdReplacement) {
    *prediction = inference_module_->PredictScore(PreprocessExample(example)) >=
                  predict_threshold_replacement;
  } else {
    *prediction = inference_module_->Predict(PreprocessExample(example));
  }
  DVLOG(1) << "Predictor " << GetModelName() << " predicted: " << *prediction;
  return true;
}

bool BinaryClassifierPredictor::PredictScore(const RankerExample& example,
                                             float* prediction) {
  if (!IsReady()) {
    DVLOG(1) << "Predictor " << GetModelName() << " not ready for prediction.";
    return false;
  }
  *prediction = inference_module_->PredictScore(PreprocessExample(example));
  DVLOG(1) << "Predictor " << GetModelName() << " predicted: " << prediction;
  return true;
}

// static
RankerModelStatus BinaryClassifierPredictor::ValidateModel(
    const RankerModel& model) {
  if (model.proto().model_case() != RankerModelProto::kLogisticRegression) {
    DVLOG(0) << "Model is incompatible.";
    return RankerModelStatus::INCOMPATIBLE;
  }
  const GenericLogisticRegressionModel& glr =
      model.proto().logistic_regression();
  if (glr.is_preprocessed_model()) {
    if (glr.fullname_weights().empty() || !glr.weights().empty()) {
      DVLOG(0) << "Model is incompatible. Preprocessed model should use "
                  "fullname_weights.";
      return RankerModelStatus::INCOMPATIBLE;
    }
    if (!glr.preprocessor_config().feature_indices().empty()) {
      DVLOG(0) << "Preprocessed model doesn't need feature indices.";
      return RankerModelStatus::INCOMPATIBLE;
    }
  } else {
    if (!glr.fullname_weights().empty() || glr.weights().empty()) {
      DVLOG(0) << "Model is incompatible. Non-preprocessed model should use "
                  "weights.";
      return RankerModelStatus::INCOMPATIBLE;
    }
  }
  return RankerModelStatus::OK;
}

bool BinaryClassifierPredictor::Initialize() {
  if (ranker_model_->proto().model_case() ==
      RankerModelProto::kLogisticRegression) {
    inference_module_ = std::make_unique<GenericLogisticRegressionInference>(
        ranker_model_->proto().logistic_regression());
    return true;
  }

  DVLOG(0) << "Could not initialize inference module.";
  return false;
}

}  // namespace assist_ranker
