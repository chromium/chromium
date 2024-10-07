// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/classifier_predictor.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/nn_classifier.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_model.h"
#include "components/assist_ranker/ranker_model_loader_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace assist_ranker {

ClassifierPredictor::ClassifierPredictor(const PredictorConfig& config)
    : BasePredictor(config) {}
ClassifierPredictor::~ClassifierPredictor() = default;

// static
std::unique_ptr<ClassifierPredictor> ClassifierPredictor::Create(
    const PredictorConfig& config,
    const base::FilePath& model_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  std::unique_ptr<ClassifierPredictor> predictor(
      new ClassifierPredictor(config));
  if (!predictor->is_query_enabled()) {
    DVLOG(1) << "Query disabled, bypassing model loading.";
    return predictor;
  }
  const GURL& model_url = predictor->GetModelUrl();
  DVLOG(1) << "Creating predictor instance for " << predictor->GetModelName();
  DVLOG(1) << "Model URL: " << model_url;
  auto model_loader = std::make_unique<RankerModelLoaderImpl>(
      base::BindRepeating(&ClassifierPredictor::ValidateModel),
      base::BindRepeating(&ClassifierPredictor::OnModelAvailable,
                          base::Unretained(predictor.get())),
      url_loader_factory, model_path, model_url, config.uma_prefix);
  predictor->LoadModel(std::move(model_loader));
  return predictor;
}

bool ClassifierPredictor::Predict(const std::vector<float>& features,
                                  std::vector<float>* prediction) {
  if (!IsReady()) {
    DVLOG(1) << "Predictor " << GetModelName() << " not ready for prediction.";
    return false;
  }

  *prediction = nn_classifier::Inference(model_, features);
  return true;
}

bool ClassifierPredictor::Predict(RankerExample example,
                                  std::vector<float>* prediction) {
  if (!IsReady()) {
    DVLOG(1) << "Predictor " << GetModelName() << " not ready for prediction.";
    return false;
  }

  if (!model_.has_preprocessor_config()) {
    DVLOG(1) << "No preprocessor config specified.";
    return false;
  }

  const int preprocessor_error =
      ExamplePreprocessor::Process(model_.preprocessor_config(), &example);

  // It is okay to ignore cases where there is an extra feature that is not in
  // the config.
  if (preprocessor_error != ExamplePreprocessor::kSuccess &&
      preprocessor_error != ExamplePreprocessor::kNoFeatureIndexFound) {
    DVLOG(1) << "Preprocessing error " << preprocessor_error;
    return false;
  }

  const auto& vec =
      example.features()
          .at(assist_ranker::ExamplePreprocessor::kVectorizedFeatureDefaultName)
          .float_list()
          .float_value();
  const std::vector<float> features(vec.begin(), vec.end());
  return Predict(features, prediction);
}

// static
RankerModelStatus ClassifierPredictor::ValidateModel(const RankerModel& model) {
  if (model.proto().model_case() != RankerModelProto::kNnClassifier) {
    DVLOG(0) << "Model is incompatible.";
    return RankerModelStatus::INCOMPATIBLE;
  }
  return nn_classifier::Validate(model.proto().nn_classifier())
             ? RankerModelStatus::OK
             : RankerModelStatus::INCOMPATIBLE;
}

bool ClassifierPredictor::Initialize() {
  if (ranker_model_->proto().model_case() == RankerModelProto::kNnClassifier) {
    model_ = ranker_model_->proto().nn_classifier();
    return true;
  }

  DVLOG(0) << "Could not initialize inference module.";
  return false;
}

}  // namespace assist_ranker
