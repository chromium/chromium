// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_model_observer.h"

#include "components/omnibox/browser/on_device_model_update_listener.h"
#include "components/optimization_guide/proto/models.pb.h"

OnDeviceTailModelObserver::OnDeviceTailModelObserver(
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : opt_guide_(opt_guide) {
  if (opt_guide_) {
    opt_guide_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
        /* model_metadata= */ absl::nullopt, this);
  }
}

OnDeviceTailModelObserver::~OnDeviceTailModelObserver() {
  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      this);
  opt_guide_ = nullptr;
}

void OnDeviceTailModelObserver::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  if (optimization_target !=
      optimization_guide::proto::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST) {
    return;
  }
  auto* listener = OnDeviceModelUpdateListener::GetInstance();
  if (listener)
    listener->OnTailModelUpdate(model_info.GetModelFilePath(),
                                model_info.GetAdditionalFiles());
}
