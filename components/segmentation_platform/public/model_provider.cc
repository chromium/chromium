// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

ModelProvider::ModelProvider(proto::SegmentId segment_id)
    : segment_id_(segment_id) {}

ModelProvider::~ModelProvider() = default;

ModelProviderFactory::~ModelProviderFactory() = default;

}  // namespace segmentation_platform
