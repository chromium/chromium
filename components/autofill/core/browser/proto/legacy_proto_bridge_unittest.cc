// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/proto/legacy_proto_bridge.h"

#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Property;

// Makes an arbitrary field metadata proto to be used for testing.
AutofillRandomizedFieldMetadata GetFieldMetadata() {
  AutofillRandomizedFieldMetadata metadata;
  AutofillRandomizedValue random_value;
  random_value.set_encoding_type(AutofillRandomizedValue::BIT_0);
  random_value.set_encoded_bits("1234");
  *metadata.mutable_id() = std::move(random_value);
  return metadata;
}

// Makes an arbitrary form metadata proto to be used for testing.
AutofillRandomizedFormMetadata GetformMetadata() {
  AutofillRandomizedFormMetadata metadata;
  AutofillRandomizedValue random_value;
  random_value.set_encoding_type(AutofillRandomizedValue::BIT_1);
  random_value.set_encoded_bits("5678");
  *metadata.mutable_id() = std::move(random_value);
  return metadata;
}

AutofillQueryContents::Form::Field MakeLegacyField(uint32_t signature,
                                                   const std::string& name,
                                                   const std::string& type) {
  AutofillQueryContents::Form::Field field;
  field.set_signature(signature);
  field.set_name(name);
  field.set_type(type);
  *field.mutable_field_metadata() = GetFieldMetadata();
  return field;
}

AutofillQueryResponse::FormSuggestion::FieldSuggestion MakeFieldSuggestion(
    uint32_t field_signature,
    uint32_t primary_type_prediction,
    std::vector<uint32_t> predictions,
    bool may_use_prefilled_placeholder,
    PasswordRequirementsSpec password_requirements) {
  AutofillQueryResponse::FormSuggestion::FieldSuggestion field_suggestion;
  field_suggestion.set_field_signature(field_signature);
  field_suggestion.set_primary_type_prediction(primary_type_prediction);
  for (auto prediction : predictions) {
    field_suggestion.add_predictions()->set_type(prediction);
  }
  field_suggestion.set_may_use_prefilled_placeholder(
      may_use_prefilled_placeholder);
  *field_suggestion.mutable_password_requirements() =
      std::move(password_requirements);
  return field_suggestion;
}

TEST(ProtoBridgeTest, TestCreateApiRequestFromLegacyRequest) {
  AutofillQueryContents legacy_request;
  legacy_request.set_client_version("dummy client v1");
  legacy_request.add_experiments(1234);
  legacy_request.add_experiments(5678);
  AutofillQueryContents::Form* new_form = legacy_request.add_form();
  new_form->set_signature(1234U);
  *new_form->mutable_form_metadata() = GetformMetadata();
  *new_form->add_field() = MakeLegacyField(1234U, "First Name", "text");
  *new_form->add_field() = MakeLegacyField(5678U, "Last Name", "text");

  new_form = legacy_request.add_form();
  new_form->set_signature(5678U);
  *new_form->mutable_form_metadata() = GetformMetadata();
  *new_form->add_field() = MakeLegacyField(1234U, "Street Address", "text");
  *new_form->add_field() = MakeLegacyField(5678U, "Zip Code", "text");

  AutofillPageQueryRequest api_request =
      CreateApiRequestFromLegacyRequest(legacy_request);

  EXPECT_EQ(api_request.client_version(), "dummy client v1");
  EXPECT_EQ(api_request.experiments(0), 1234);
  EXPECT_EQ(api_request.experiments(1), 5678);
  EXPECT_EQ(api_request.forms(0).signature(), 1234U);
  EXPECT_EQ(api_request.forms(0).metadata().id().encoding_type(),
            AutofillRandomizedValue::BIT_1);
  EXPECT_EQ(api_request.forms(0).metadata().id().encoded_bits(), "5678");
  EXPECT_EQ(api_request.forms(1).signature(), 5678U);
  EXPECT_EQ(api_request.forms(1).metadata().id().encoding_type(),
            AutofillRandomizedValue::BIT_1);
  EXPECT_EQ(api_request.forms(1).metadata().id().encoded_bits(), "5678");
  // Assert fields of form 0.
  EXPECT_EQ(api_request.forms(0).fields(0).signature(), 1234U);
  EXPECT_EQ(api_request.forms(0).fields(0).name(), "First Name");
  EXPECT_EQ(api_request.forms(0).fields(0).control_type(), "text");
  EXPECT_EQ(api_request.forms(0).fields(0).metadata().id().encoding_type(),
            AutofillRandomizedValue::BIT_0);
  EXPECT_EQ(api_request.forms(0).fields(0).metadata().id().encoded_bits(),
            "1234");
  EXPECT_EQ(api_request.forms(0).fields(1).signature(), 5678U);
  EXPECT_EQ(api_request.forms(0).fields(1).name(), "Last Name");
  EXPECT_EQ(api_request.forms(0).fields(1).control_type(), "text");
  EXPECT_EQ(api_request.forms(0).fields(1).metadata().id().encoding_type(),
            AutofillRandomizedValue::BIT_0);
  EXPECT_EQ(api_request.forms(0).fields(1).metadata().id().encoded_bits(),
            "1234");
  // Assert fields of form 1.
  EXPECT_EQ(api_request.forms(1).fields(0).signature(), 1234U);
  EXPECT_EQ(api_request.forms(1).fields(0).name(), "Street Address");
  EXPECT_EQ(api_request.forms(1).fields(0).control_type(), "text");
  EXPECT_EQ(api_request.forms(1).fields(0).metadata().id().encoding_type(),
            AutofillRandomizedValue::BIT_0);
  EXPECT_EQ(api_request.forms(1).fields(0).metadata().id().encoded_bits(),
            "1234");
  EXPECT_EQ(api_request.forms(1).fields(1).signature(), 5678U);
  EXPECT_EQ(api_request.forms(1).fields(1).name(), "Zip Code");
  EXPECT_EQ(api_request.forms(1).fields(1).control_type(), "text");
  EXPECT_EQ(api_request.forms(1).fields(1).metadata().id().encoding_type(),
            AutofillRandomizedValue::BIT_0);
  EXPECT_EQ(api_request.forms(1).fields(1).metadata().id().encoded_bits(),
            "1234");
}

TEST(ProtoBridgeTest, CreateLegacyResponseFromApiResponse) {
  constexpr uint32_t dummy_password_type = 1U;
  constexpr uint32_t dummy_address_type = 2U;
  constexpr uint32_t dummy_password_priority = 3U;

  PasswordRequirementsSpec dummy_password_requirement_specs;
  dummy_password_requirement_specs.set_priority(dummy_password_priority);

  AutofillQueryResponse api_response;
  // Add suggestions for form 0.
  auto* form_suggestion = api_response.add_form_suggestions();
  *form_suggestion->add_field_suggestions() = MakeFieldSuggestion(
      /*field_signature=*/1234U,
      /*primary_type_prediction=*/dummy_password_type,
      /*predictions=*/{dummy_password_type, dummy_address_type},
      /*may_use_prefilled_placeholder=*/true,
      /*password_requirements=*/dummy_password_requirement_specs);
  // Add suggestions for form 1.
  form_suggestion = api_response.add_form_suggestions();
  *form_suggestion->add_field_suggestions() = MakeFieldSuggestion(
      /*field_signature=*/5678U, /*primary_type_prediction=*/dummy_address_type,
      /*predictions=*/{dummy_address_type, dummy_password_type},
      /*may_use_prefilled_placeholder=*/false,
      /*password_requirements=*/dummy_password_requirement_specs);

  AutofillQueryResponseContents legacy_response =
      CreateLegacyResponseFromApiResponse(api_response);

  // Assert fields of form 0 in legacy response.
  EXPECT_EQ(legacy_response.field(0).overall_type_prediction(),
            dummy_password_type);
  EXPECT_THAT(
      legacy_response.field(0).predictions(),
      ElementsAre(
          Property(&AutofillQueryResponseContents::Field::FieldPrediction::type,
                   Eq(dummy_password_type)),
          Property(&AutofillQueryResponseContents::Field::FieldPrediction::type,
                   Eq(dummy_address_type))));
  EXPECT_THAT(legacy_response.field(0).password_requirements(),
              Property(&PasswordRequirementsSpec::priority,
                       Eq(dummy_password_priority)));

  // Assert fields of form 1 in legacy response.
  EXPECT_EQ(legacy_response.field(1).overall_type_prediction(),
            dummy_address_type);
  EXPECT_THAT(
      legacy_response.field(1).predictions(),
      ElementsAre(
          Property(&AutofillQueryResponseContents::Field::FieldPrediction::type,
                   Eq(dummy_address_type)),
          Property(&AutofillQueryResponseContents::Field::FieldPrediction::type,
                   Eq(dummy_password_type))));
  EXPECT_THAT(legacy_response.field(1).password_requirements(),
              Property(&PasswordRequirementsSpec::priority,
                       Eq(dummy_password_priority)));
}

}  // namespace
}  // namespace autofill
