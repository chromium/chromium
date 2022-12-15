// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

ClassificationResult::ClassificationResult(PredictionStatus status)
    : status(status) {}

ClassificationResult::~ClassificationResult() = default;

ClassificationResult::ClassificationResult(const ClassificationResult&) =
    default;

ClassificationResult& ClassificationResult::operator=(
    const ClassificationResult&) = default;

RegressionResult::RegressionResult(PredictionStatus status) : status(status) {}

RegressionResult::~RegressionResult() = default;

RegressionResult::RegressionResult(const RegressionResult&) = default;

RegressionResult& RegressionResult::operator=(const RegressionResult&) =
    default;

}  // namespace segmentation_platform
