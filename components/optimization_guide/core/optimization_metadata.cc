// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_metadata.h"

namespace optimization_guide {

OptimizationMetadata::OptimizationMetadata() = default;
OptimizationMetadata::~OptimizationMetadata() = default;
OptimizationMetadata::OptimizationMetadata(const OptimizationMetadata&) =
    default;

void OptimizationMetadata::SetAnyMetadataForTesting(
    const google::protobuf::MessageLite& metadata) {
  proto::Any any;
  any.set_type_url(metadata.GetTypeName());
  metadata.SerializeToString(any.mutable_value());
  any_metadata_ = any;
}

}  // namespace optimization_guide
