// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_

#include "components/optimization_guide/proto/descriptors.pb.h"
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

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FEATURE_CONFIG_BUILDER_H_
