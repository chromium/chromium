// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_COMMON_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_COMMON_H_

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace network {
struct ResourceRequest;
}  // namespace network

// This file exclusively holds common methods shared between different paths for
// remote model execution. Utility methods of other purpose should not be added
// here.
namespace optimization_guide {

// The name of the model execution debug logs header.
inline constexpr char kOptimizationGuideModelExecutionDebugLogsHeaderKey[] =
    "X-Model-Execution-Debug-Logs";

// Adds header to indicate to return debug logging data from the model execution
// service via response header.
inline constexpr char kModelExecutionEnableRemoteDebugLoggingSwitch[] =
    "optimization-guide-model-execution-enable-remote-debug-logging";

// Returns an ExecuteRequest proto populated with `feature` and serialized
// `request_metadata` wrapped in an Any proto.
proto::ExecuteRequest CreateExecuteRequest(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata);

// Returns the NetworkTrafficAnnotationTag for the given `feature`.
net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    ModelBasedCapabilityKey feature);

// Returns whether model executions for the `feature` require an access token.
bool IsAccessTokenRequiredForFeature(ModelBasedCapabilityKey feature);

// Appends headers as specified by the command line arguments.
void AppendHeadersIfNeeded(network::ResourceRequest& request);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REMOTE_MODEL_EXECUTION_COMMON_H_
