// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/mock_feature_list_query_processor.h"

namespace segmentation_platform {

MockFeatureListQueryProcessor::MockFeatureListQueryProcessor()
    : FeatureListQueryProcessor(nullptr, nullptr) {}

MockFeatureListQueryProcessor::~MockFeatureListQueryProcessor() = default;

}  // namespace segmentation_platform
