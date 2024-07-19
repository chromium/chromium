// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"

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
  proto::ProtoField result;
  result.add_proto_descriptors()->set_tag_number(3);
  result.add_proto_descriptors()->set_tag_number(1);
  return result;
}

proto::ProtoField UserInputField() {
  proto::ProtoField result;
  result.add_proto_descriptors()->set_tag_number(7);
  result.add_proto_descriptors()->set_tag_number(1);
  return result;
}

proto::ProtoField PreviousResponseField() {
  proto::ProtoField result;
  result.add_proto_descriptors()->set_tag_number(8);
  result.add_proto_descriptors()->set_tag_number(1);
  return result;
}

proto::ProtoField OutputField() {
  proto::ProtoField result;
  result.add_proto_descriptors()->set_tag_number(1);
  return result;
}

proto::ProtoField StringValueField() {
  proto::ProtoField result;
  result.add_proto_descriptors()->set_tag_number(1);
  return result;
}

proto::SubstitutedString FieldSubstitution(const std::string& tmpl,
                                           proto::ProtoField&& field) {
  proto::SubstitutedString result;
  result.set_string_template(tmpl);
  *result.add_substitutions()->add_candidates()->mutable_proto_field() = field;
  return result;
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
      ToModelExecutionFeatureProto(ModelBasedCapabilityKey::kCompose));
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

}  // namespace optimization_guide
