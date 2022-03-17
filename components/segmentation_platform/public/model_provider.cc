// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

ModelProvider::ModelProvider(
    optimization_guide::proto::OptimizationTarget optimization_target)
    : optimization_target_(optimization_target) {}

ModelProvider::~ModelProvider() = default;

ModelProviderFactory::~ModelProviderFactory() = default;

}  // namespace segmentation_platform
