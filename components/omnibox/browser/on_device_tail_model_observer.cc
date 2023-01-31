// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_model_observer.h"

#include "base/logging.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/on_device_tail_suggest_model_metadata.pb.h"

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
  if (listener) {
    absl::optional<optimization_guide::proto::OnDeviceTailSuggestModelMetadata>
        metadata = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::OnDeviceTailSuggestModelMetadata>(
            model_info.GetModelMetadata().value());

    if (!metadata.has_value()) {
      DVLOG(1) << "Failed to fetch metadata for Omnibox on device tail model";
      return;
    }

    listener->OnTailModelUpdate(model_info.GetModelFilePath(),
                                model_info.GetAdditionalFiles(),
                                metadata.value());
  }
}
