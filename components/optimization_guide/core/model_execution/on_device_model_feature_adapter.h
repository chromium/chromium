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
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/text_safety.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-forward.h"

namespace optimization_guide {

class Redactor;
class ResponseParser;

// Adapts the on-device model to be used for a particular feature, based on
// a configuration proto.
class OnDeviceModelFeatureAdapter final
    : public base::RefCounted<OnDeviceModelFeatureAdapter> {
 public:
  using ResponseParserFactory =
      base::RepeatingCallback<std::unique_ptr<ResponseParser>(
          const proto::OnDeviceModelExecutionOutputConfig&)>;

  // Constructs an adapter from a configuration proto.
  explicit OnDeviceModelFeatureAdapter(
      proto::OnDeviceModelExecutionFeatureConfig config,
      // Allows dependency injection for use in tests.
      ResponseParserFactory response_parser_factory = ResponseParserFactory());

  // Constructs the model input from `request`.
  std::optional<SubstitutionResult> ConstructInputString(
      MultimodalMessageReadView request,
      bool want_input_context) const;

  bool ShouldParseResponse(ResponseCompleteness completeness) const;

  // Converts model response into this feature's expected response type.
  // Replies with std::nullopt on error.
  // The `previous_response_pos` might be used by the parser to determine which
  // part of the response to return to the responder.
  void ParseResponse(const MultimodalMessage& request,
                     const std::string& model_response,
                     size_t previous_response_pos,
                     ResponseParser::ResultCallback callback) const;

  // Constructs the request for text safety server fallback.
  // Will return std::nullopt on error or if the config does not allow for it.
  std::optional<proto::TextSafetyRequest> ConstructTextSafetyRequest(
      MultimodalMessageReadView request,
      const std::string& text) const;

  bool CanSkipTextSafety() const { return config_.can_skip_text_safety(); }

  SamplingParamsConfig GetSamplingParamsConfig() const;

  const proto::Any& GetFeatureMetadata() const;

  const TokenLimits& GetTokenLimits() const;

  const proto::OnDeviceModelExecutionFeatureConfig& config() const {
    return config_;
  }

  // Get the configured response constraint, may be null.
  on_device_model::mojom::ResponseConstraintPtr GetResponseConstraint() const;

 private:
  friend class base::RefCounted<OnDeviceModelFeatureAdapter>;
  ~OnDeviceModelFeatureAdapter();

  // Redacts the content of current response, given the last executed message.
  RedactResult Redact(MultimodalMessageReadView last_message,
                      std::string& current_response) const;

  // Returns the string that is used for checking redaction against.
  std::string GetStringToCheckForRedacting(
      MultimodalMessageReadView message) const;

  proto::OnDeviceModelExecutionFeatureConfig config_;
  TokenLimits token_limits_;
  Redactor redactor_;
  std::unique_ptr<ResponseParser> parser_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_FEATURE_ADAPTER_H_
