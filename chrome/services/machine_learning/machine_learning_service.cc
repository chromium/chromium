// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/machine_learning_service.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/services/machine_learning/decision_tree_predictor.h"
#include "chrome/services/machine_learning/metrics.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace machine_learning {

MachineLearningService::MachineLearningService(
    mojo::PendingReceiver<mojom::MachineLearningService> receiver)
    : receiver_(this, std::move(receiver)) {}

MachineLearningService::~MachineLearningService() = default;

void MachineLearningService::LoadDecisionTree(
    mojom::DecisionTreeModelSpecPtr spec,
    mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
    LoadDecisionTreeCallback callback) {
  auto predictor = DecisionTreePredictor::FromModelSpec(std::move(spec));
  if (predictor->IsValid()) {
    mojo::MakeSelfOwnedReceiver(std::move(predictor), std::move(receiver));
    UMA_HISTOGRAM_ENUMERATION(metrics::kDecisionTreeModelLoadResult,
                              mojom::LoadModelResult::kOk);
    std::move(callback).Run(mojom::LoadModelResult::kOk);
  } else {
    UMA_HISTOGRAM_ENUMERATION(metrics::kDecisionTreeModelLoadResult,
                              mojom::LoadModelResult::kModelSpecError);
    std::move(callback).Run(mojom::LoadModelResult::kModelSpecError);
  }
}

}  // namespace machine_learning
