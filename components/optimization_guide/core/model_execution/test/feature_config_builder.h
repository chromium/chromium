// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_

#include <initializer_list>
#include <optional>
#include <string>

#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/redaction.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"

namespace optimization_guide {

// Sets a threshold that will reject text containing "unsafe"  when used with
// FakeOnDeviceModel::ClassifyTextSafety.
proto::SafetyCategoryThreshold ForbidUnsafe();

// Sets a threshold that will reject text without "reasonable" when used with
// FakeOnDeviceModel::ClassifyTextSafety.
proto::SafetyCategoryThreshold RequireReasonable();

// Reference ComposeRequest::page_metadata.page_url
proto::ProtoField PageUrlField();

// Reference ComposeRequest::generate_params.user_input
proto::ProtoField UserInputField();

// Reference ComposeRequest::rewrite_params.previous_response
proto::ProtoField PreviousResponseField();

// Reference ComposeResponse::output
proto::ProtoField OutputField();

// Reference StringValue::value
proto::ProtoField StringValueField();

// Make a template for "url: {page_url}".
proto::SubstitutedString PageUrlSubstitution();

// Constructs a simple redact rule, which exempts text from UserInput.
proto::RedactRules SimpleRedactRule(
    const std::string& regex,
    proto::RedactBehavior behavior =
        proto::RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT,
    std::optional<std::string> replacement = std::nullopt);

// Constructs a simple compose config.
// Generates "ctx:{user input}" and "execute:{user input}{page_url}".
// Outputs to a ComposeResponse::output field.
proto::OnDeviceModelExecutionFeatureConfig SimpleComposeConfig();

// Trivial safety config for compose with no checks.
proto::FeatureTextSafetyConfiguration ComposeSafetyConfig();

// Returns a validation config that passes with the default model settings.
inline proto::OnDeviceModelValidationConfig WillPassValidationConfig() {
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  // This prompt passes because by default the model will echo the input.
  prompt->set_prompt("hElLo");
  prompt->set_expected_output("HeLlO");
  return validation_config;
}

// Returns a validation config that fails with the default model settings.
inline proto::OnDeviceModelValidationConfig WillFailValidationConfig() {
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  // This prompt fails because by default the model will echo the input.
  prompt->set_prompt("hello");
  prompt->set_expected_output("goodbye");
  return validation_config;
}

inline proto::OnDeviceModelExecutionConfig ExecutionConfigWithValidation(
    proto::OnDeviceModelValidationConfig validation_config) {
  proto::OnDeviceModelExecutionConfig cfg;
  *cfg.mutable_validation_config() = std::move(validation_config);
  return cfg;
}

inline proto::OnDeviceModelExecutionConfig ExecutionConfigWithCapabilities(
    const std::vector<proto::OnDeviceModelCapability>& capabilities) {
  proto::OnDeviceModelExecutionConfig cfg;
  for (proto::OnDeviceModelCapability c : capabilities) {
    cfg.add_capabilities(c);
  }
  return cfg;
}

// Construct an InputConfig that formats a proto::ExampleForTestingRequest.
proto::OnDeviceModelExecutionInputConfig TestInputConfig(
    proto::SubstitutedString context_template,
    proto::SubstitutedString execution_template);

// Construct an output config compatible with ResponseHolder.
proto::OnDeviceModelExecutionOutputConfig ResponseHolderOutputConfig();

// Construct a SubstitutedString that repeats a chunk for each element in
// proto::ExampleForTestingRequest::repeated
inline proto::SubstitutedString ForEachRepeated(proto::SubstitutedString sub) {
  return ForEachSubstitution(
      ProtoField({proto::ExampleForTestingRequest::kRepeatedFieldFieldNumber}),
      std::move(sub));
}

// A StringSubstitution that formats an ExampleForTestingMessage.
proto::SubstitutedString FormatTestMessage();

// Construct a TextSafetyModelMetadata that composes the feature configs.
proto::TextSafetyModelMetadata SafetyMetadata(
    std::initializer_list<proto::FeatureTextSafetyConfiguration> configs);

// Constructs a simple config for the Test feature.
// Formats inputs from ExampleForTestingMessage.
// Outputs to a ComposeResponse::output field.
proto::OnDeviceModelExecutionFeatureConfig SimpleTestFeatureConfig();

// Constructs OnDeviceBaseModelMetadata for test assets.
inline proto::OnDeviceBaseModelMetadata CreateOnDeviceBaseModelMetadata(
    const std::string& base_model_name,
    const std::string& base_model_version,
    std::vector<proto::OnDeviceModelPerformanceHint> hints) {
  proto::OnDeviceBaseModelMetadata model_metadata;
  model_metadata.set_base_model_name(base_model_name);
  model_metadata.set_base_model_version(base_model_version);
  *model_metadata.mutable_supported_performance_hints() = {hints.begin(),
                                                           hints.end()};
  return model_metadata;
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_
