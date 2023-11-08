// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_

#include <optional>

#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// Returns the value of `proto_field` from `msg`.
std::optional<proto::Value> GetProtoValue(
    const google::protobuf::MessageLite& msg,
    const optimization_guide::proto::ProtoField& proto_field);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_PROTO_DESCRIPTORS_H_
