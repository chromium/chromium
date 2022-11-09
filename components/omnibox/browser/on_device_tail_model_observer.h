// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_OBSERVER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

// The optimization target model observer implementation to update the files
// for Omnibox on device tail suggest.
class OnDeviceTailModelObserver
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  explicit OnDeviceTailModelObserver(
      optimization_guide::OptimizationGuideModelProvider* opt_guide);
  ~OnDeviceTailModelObserver() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

 private:
  // Optimization Guide Service that provides model files for this service.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_ =
      nullptr;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_OBSERVER_H_
