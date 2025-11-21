// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"

#include <initializer_list>

#include "base/strings/string_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"

namespace optimization_guide {

proto::SafetyCategoryThreshold ForbidUnsafe() {
  proto::SafetyCategoryThreshold result;
  result.set_output_index(0);  // FakeOnDeviceModel's "SAFETY" category.
  result.set_threshold(0.5);
  return result;
}

proto::SafetyCategoryThreshold RequireReasonable() {
  proto::SafetyCategoryThreshold result;
  result.set_output_index(1);  // FakeOnDeviceModel's "REASONABLE" category.
  result.set_threshold(0.5);
  return result;
}

proto::ProtoField PageUrlField() {
  return ProtoField({3, 1});
}

proto::ProtoField UserInputField() {
  return ProtoField({7, 1});
}

proto::ProtoField PreviousResponseField() {
  return ProtoField({8, 1});
}

proto::ProtoField OutputField() {
  return ProtoField({1});
}

proto::ProtoField StringValueField() {
  return ProtoField({1});
}

proto::SubstitutedString PageUrlSubstitution() {
  return FieldSubstitution("url: %s", PageUrlField());
}

proto::RedactRules SimpleRedactRule(const std::string& regex,
                                    proto::RedactBehavior behavior,
                                    std::optional<std::string> replacement) {
  proto::RedactRules redact_rules;
  redact_rules.mutable_fields_to_check()->Add(UserInputField());
  auto& redact_rule = *redact_rules.add_rules();
  redact_rule.set_regex(regex);
  redact_rule.set_behavior(behavior);
  if (replacement) {
    redact_rule.set_replacement_string(*replacement);
  }
  return redact_rules;
}

proto::OnDeviceModelExecutionFeatureConfig SimpleComposeConfig() {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(
      ToModelExecutionFeatureProto(mojom::OnDeviceFeature::kCompose));
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name(proto::ComposeRequest().GetTypeName());

  // Execute call prefixes with execute:.
  auto& substitution = *input_config.add_execute_substitutions();
  substitution.set_string_template("execute:%s%s");
  *substitution.add_substitutions()->add_candidates()->mutable_proto_field() =
      UserInputField();
  *substitution.add_substitutions()->add_candidates()->mutable_proto_field() =
      PageUrlField();

  // Context call prefixes with context:.
  auto& context_substitution = *input_config.add_input_context_substitutions();
  context_substitution.set_string_template("ctx:%s");
  *context_substitution.add_substitutions()
       ->add_candidates()
       ->mutable_proto_field() = UserInputField();

  auto& output_config = *config.mutable_output_config();
  output_config.set_proto_type(proto::ComposeResponse().GetTypeName());
  *output_config.mutable_proto_field() = OutputField();
  return config;
}

proto::FeatureTextSafetyConfiguration ComposeSafetyConfig() {
  proto::FeatureTextSafetyConfiguration config;
  config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  return config;
}

proto::TextSafetyModelMetadata SafetyMetadata(
    std::initializer_list<proto::FeatureTextSafetyConfiguration> configs) {
  proto::TextSafetyModelMetadata metadata;
  for (auto& cfg : configs) {
    *metadata.add_feature_text_safety_configurations() = std::move(cfg);
  }
  return metadata;
}

proto::OnDeviceModelExecutionInputConfig TestInputConfig(
    proto::SubstitutedString context_template,
    proto::SubstitutedString execution_template) {
  proto::OnDeviceModelExecutionInputConfig input_config;
  input_config.set_request_base_name(
      proto::ExampleForTestingRequest().GetTypeName());
  *input_config.add_input_context_substitutions() = std::move(context_template);
  *input_config.add_execute_substitutions() = std::move(execution_template);
  return input_config;
}

proto::OnDeviceModelExecutionOutputConfig ResponseHolderOutputConfig() {
  proto::OnDeviceModelExecutionOutputConfig output_config;
  output_config.set_proto_type(proto::ComposeResponse().GetTypeName());
  *output_config.mutable_proto_field() =
      ProtoField({proto::ComposeResponse::kOutputFieldNumber});
  return output_config;
}

proto::SubstitutedString FormatTestMessage() {
  using Msg = proto::ExampleForTestingMessage;
  proto::SubstitutedString result;
  result.set_string_template("%s%s");
  *result.add_substitutions()->add_candidates()->mutable_proto_field() =
      ProtoField({Msg::kStringValueFieldNumber});
  *result.add_substitutions()
       ->add_candidates()
       ->mutable_media_field()
       ->mutable_proto_field() = ProtoField({Msg::kMediaFieldNumber});
  return result;
}

proto::OnDeviceModelExecutionFeatureConfig SimpleTestFeatureConfig() {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(
      ToModelExecutionFeatureProto(mojom::OnDeviceFeature::kTest));
  *config.mutable_input_config() =
      TestInputConfig(FormatTestMessage(), FormatTestMessage());
  *config.mutable_output_config() = ResponseHolderOutputConfig();
  return config;
}

}  // namespace optimization_guide
