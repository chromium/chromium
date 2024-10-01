// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_FEATURE_ADAPTER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_FEATURE_ADAPTER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/text_safety.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

class Redactor;

// Adapts the on-device model to be used for a particular feature, based on
// a configuration proto.
class OnDeviceModelFeatureAdapter final
    : public base::RefCounted<OnDeviceModelFeatureAdapter> {
 public:
  // Constructs an adapter from a configuration proto.
  explicit OnDeviceModelFeatureAdapter(
      proto::OnDeviceModelExecutionFeatureConfig&& config);

  // Constructs the model input from `request`.
  std::optional<SubstitutionResult> ConstructInputString(
      const google::protobuf::MessageLite& request,
      bool want_input_context) const;

  bool ShouldParseResponse(bool is_complete) const;

  // Converts model response into this feature's expected response type.
  // Replies with std::nullopt on error.
  void ParseResponse(const google::protobuf::MessageLite& request,
                     const std::string& model_response,
                     ResponseParser::ResultCallback callback) const;

  // Constructs the request for text safety server fallback.
  // Will return std::nullopt on error or if the config does not allow for it.
  std::optional<proto::TextSafetyRequest> ConstructTextSafetyRequest(
      const google::protobuf::MessageLite& request,
      const std::string& text) const;

  bool CanSkipTextSafety() const { return config_.can_skip_text_safety(); }

  std::optional<SamplingParams> MaybeSamplingParams() const;

  const proto::Any& GetFeatureMetadata() const;

 private:
  friend class base::RefCounted<OnDeviceModelFeatureAdapter>;
  ~OnDeviceModelFeatureAdapter();

  // Redacts the content of current response, given the last executed message.
  RedactResult Redact(const google::protobuf::MessageLite& last_message,
                      std::string& current_response) const;

  // Returns the string that is used for checking redaction against.
  std::string GetStringToCheckForRedacting(
      const google::protobuf::MessageLite& message) const;

  proto::OnDeviceModelExecutionFeatureConfig config_;
  Redactor redactor_;
  std::unique_ptr<ResponseParser> parser_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_FEATURE_ADAPTER_H_
