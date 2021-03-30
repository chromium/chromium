// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model.h"

#include <utility>

#include "components/optimization_guide/core/decision_tree_prediction_model.h"

namespace optimization_guide {

// static
std::unique_ptr<PredictionModel> PredictionModel::Create(
    const proto::PredictionModel& prediction_model) {
  // TODO(crbug/1009123): Add a histogram to record if the provided model is
  // constructed successfully or not.
  // TODO(crbug/1009123): Adding timing metrics around initialization due to
  // potential validation overhead.
  if (!prediction_model.has_model())
    return nullptr;

  if (!prediction_model.has_model_info())
    return nullptr;

  if (!prediction_model.model_info().has_version())
    return nullptr;

  // Enforce that only one ModelType is specified for the PredictionModel.
  if (prediction_model.model_info().supported_model_types_size() != 1) {
    return nullptr;
  }

  // Check that the client supports this type of model and is not an unknown
  // type.
  if (!proto::ModelType_IsValid(
          prediction_model.model_info().supported_model_types(0)) ||
      prediction_model.model_info().supported_model_types(0) ==
          proto::ModelType::MODEL_TYPE_UNKNOWN) {
    return nullptr;
  }

  // Check that the client supports the model features for |prediction model|.
  for (const auto& model_feature :
       prediction_model.model_info().supported_model_features()) {
    if (!proto::ClientModelFeature_IsValid(model_feature) ||
        model_feature ==
            proto::ClientModelFeature::CLIENT_MODEL_FEATURE_UNKNOWN)
      return nullptr;
  }

  std::unique_ptr<PredictionModel> model;
  // The Decision Tree model type is currently the only supported model type.
  if (prediction_model.model_info().supported_model_types(0) !=
      proto::ModelType::MODEL_TYPE_DECISION_TREE) {
    return nullptr;
  }
  model = std::make_unique<DecisionTreePredictionModel>(prediction_model);

  // Any constructed model must be validated for correctness according to its
  // model type before being returned.
  if (!model->ValidatePredictionModel())
    return nullptr;

  return model;
}

namespace {

std::vector<std::string> ComputeModelFeatures(
    const proto::ModelInfo& model_info) {
  std::vector<std::string> features;
  features.reserve(model_info.supported_model_features_size() +
                   model_info.supported_host_model_features_size());
  // Insert all the client model features for the owned |model_|.
  for (const auto& client_model_feature :
       model_info.supported_model_features()) {
    features.push_back(proto::ClientModelFeature_Name(client_model_feature));
  }
  // Insert all the host model features for the owned |model_|.
  for (const auto& host_model_feature :
       model_info.supported_host_model_features()) {
    features.push_back(host_model_feature);
  }
  return features;
}

}  // namespace

PredictionModel::PredictionModel(const proto::PredictionModel& prediction_model)
    : model_(prediction_model.model()),
      model_features_(ComputeModelFeatures(prediction_model.model_info())),
      version_(prediction_model.model_info().version()) {}

PredictionModel::~PredictionModel() = default;

}  // namespace optimization_guide
