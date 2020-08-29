// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/test_support/fake_service_connection.h"

#include <string>
#include <utility>
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace machine_learning {
namespace testing {

FakeServiceConnection::FakeServiceConnection() {
  ServiceConnection::SetServiceConnectionForTesting(this);
}

FakeServiceConnection::~FakeServiceConnection() {
  FakeServiceConnection::SetServiceConnectionForTesting(nullptr);
}

void FakeServiceConnection::ScheduleCall(base::OnceClosure callback) {
  if (!is_async_) {
    std::move(callback).Run();
    return;
  }

  pending_calls_.emplace_back(std::move(callback));
}

void FakeServiceConnection::RunScheduledCalls() {
  for (auto& call : pending_calls_) {
    std::move(call).Run();
  }

  pending_calls_.clear();
}

void FakeServiceConnection::SetLoadModelResult(mojom::LoadModelResult result) {
  load_model_result_ = result;
}

void FakeServiceConnection::SetDecisionTreePredictionResult(
    mojom::DecisionTreePredictionResult result,
    double prediction_score) {
  if (result == mojom::DecisionTreePredictionResult::kUnknown)
    prediction_score = 0.0;

  decision_tree_prediction_result_ = result;
  decision_tree_prediction_score_ = prediction_score;
}

void FakeServiceConnection::SetAsyncModeForTesting(bool is_async) {
  is_async_ = is_async;
}

bool FakeServiceConnection::is_service_running() const {
  return is_service_running_;
}

mojom::MachineLearningService* FakeServiceConnection::GetService() {
  if (!is_service_running_) {
    is_service_running_ = true;
  }

  return this;
}

void FakeServiceConnection::ResetServiceForTesting() {
  if (is_service_running_) {
    is_service_running_ = false;
    decision_tree_receivers_.Clear();
    pending_calls_.clear();
    load_model_result_ = mojom::LoadModelResult::kLoadModelError;
    decision_tree_prediction_result_ =
        mojom::DecisionTreePredictionResult::kUnknown;
    decision_tree_prediction_score_ = 0.0;
  }
}

void FakeServiceConnection::LoadDecisionTreeModel(
    mojom::DecisionTreeModelSpecPtr spec,
    mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
    LoadDecisionTreeCallback callback) {
  GetService();
  LoadDecisionTree(std::move(spec), std::move(receiver), std::move(callback));
}

void FakeServiceConnection::LoadDecisionTree(
    mojom::DecisionTreeModelSpecPtr spec,
    mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
    LoadDecisionTreeCallback callback) {
  if (!is_service_running_)
    return;

  ScheduleCall(base::BindOnce(&FakeServiceConnection::HandleLoadDecisionTree,
                              base::Unretained(this), std::move(receiver),
                              std::move(callback)));
}

void FakeServiceConnection::HandleLoadDecisionTree(
    mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
    LoadDecisionTreeCallback callback) {
  if (!is_service_running_)
    return;

  if (load_model_result_ == mojom::LoadModelResult::kOk)
    decision_tree_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(load_model_result_);
}

void FakeServiceConnection::Predict(
    const base::flat_map<std::string, float>& model_features,
    PredictCallback callback) {
  if (!is_service_running_)
    return;

  ScheduleCall(base::BindOnce(&FakeServiceConnection::HandleDecisionTreePredict,
                              base::Unretained(this), std::move(callback)));
}

void FakeServiceConnection::HandleDecisionTreePredict(
    PredictCallback callback) {
  if (!is_service_running_)
    return;

  std::move(callback).Run(decision_tree_prediction_result_,
                          decision_tree_prediction_score_);
}

}  // namespace testing
}  // namespace machine_learning
