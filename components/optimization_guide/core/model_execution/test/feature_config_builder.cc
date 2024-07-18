// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"

#include "components/optimization_guide/proto/descriptors.pb.h"
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

}  // namespace optimization_guide
