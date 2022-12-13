// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

ClassificationResult::ClassificationResult(PredictionStatus status)
    : status(status) {}

ClassificationResult::~ClassificationResult() = default;

RegressionResult::RegressionResult(PredictionStatus status) : status(status) {}

RegressionResult::~RegressionResult() = default;

}  // namespace segmentation_platform