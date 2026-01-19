// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

constexpr char kGoogleAPITypeName[] = "type.googleapis.com/";

namespace optimization_guide {

proto::ExecuteRequest ModelExecutionFetcher::ToExecuteRequest(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata) {
  proto::ExecuteRequest execute_request;
  execute_request.set_feature(ToModelExecutionFeatureProto(feature));
  proto::Any* any_metadata = execute_request.mutable_request_metadata();
  any_metadata->set_type_url(
      base::StrCat({kGoogleAPITypeName, request_metadata.GetTypeName()}));
  request_metadata.SerializeToString(any_metadata->mutable_value());
  return execute_request;
}

}  // namespace optimization_guide
