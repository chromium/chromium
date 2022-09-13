// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"

#include "components/segmentation_platform/public/input_delegate.h"

namespace segmentation_platform::processing {

MockFeatureListQueryProcessor::MockFeatureListQueryProcessor()
    : FeatureListQueryProcessor(nullptr, nullptr, nullptr) {}

MockFeatureListQueryProcessor::~MockFeatureListQueryProcessor() = default;

}  // namespace segmentation_platform::processing
