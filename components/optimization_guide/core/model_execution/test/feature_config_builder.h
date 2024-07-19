// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_

#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/redaction.pb.h"
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

// Make Substitution putting 'field' in 'tmpl'.
proto::SubstitutedString FieldSubstitution(const std::string& tmpl,
                                           proto::ProtoField&& field);

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

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_
