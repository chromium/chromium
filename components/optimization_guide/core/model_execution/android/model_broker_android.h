// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_

#include <memory>

#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_broker_impl.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

class ModelBrokerAndroid;

// A implementation of ModelBroker for Android.
// This object is analogous to ModelBrokerState.
// TODO(crbug.com/442912443) - Instantiate this in chrome/browser.
class ModelBrokerAndroid final {
 public:
  class SolutionFactory;

  explicit ModelBrokerAndroid(PrefService& local_state,
                              OptimizationGuideModelProvider& model_provider);
  ~ModelBrokerAndroid();

  void BindBroker(mojo::PendingReceiver<mojom::ModelBroker> receiver);

 private:
  // Initialize SolutionFactory, if not already initialized.
  void EnsureSolutionFactory(base::OnceClosure done_callback);

  raw_ref<OptimizationGuideModelProvider> model_provider_;

  // Tracks which assets have been used recently.
  UsageTracker usage_tracker_;

  // Tracks and updates the broker subscribers.
  ModelBrokerImpl impl_;

  // The factory for building solutions.
  // Creating this object may be expensive, so it is lazy-initialized.
  // Must be destroyed before usage_tracker.
  std::unique_ptr<SolutionFactory> solution_factory_;

  base::WeakPtrFactory<ModelBrokerAndroid> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ANDROID_MODEL_BROKER_ANDROID_H_
