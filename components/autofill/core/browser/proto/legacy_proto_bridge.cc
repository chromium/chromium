// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/proto/legacy_proto_bridge.h"

namespace autofill {

namespace {

AutofillPageQueryRequest::Form::Field CreateLegacyFieldFromApiField(
    const AutofillQueryContents::Form::Field& legacy_field) {
  AutofillPageQueryRequest::Form::Field api_field;
  api_field.set_signature(legacy_field.signature());
  api_field.set_name(legacy_field.name());
  api_field.set_control_type(legacy_field.type());
  *api_field.mutable_metadata() = legacy_field.field_metadata();
  return api_field;
}

AutofillPageQueryRequest::Form CreateApiFormFromLegacyForm(
    const AutofillQueryContents::Form& legacy_form) {
  AutofillPageQueryRequest::Form api_form;
  api_form.set_signature(legacy_form.signature());
  *api_form.mutable_metadata() = legacy_form.form_metadata();
  for (const auto& legacy_field : legacy_form.field()) {
    *api_form.add_fields() = CreateLegacyFieldFromApiField(legacy_field);
  }
  return api_form;
}

AutofillQueryResponseContents::Field::FieldPrediction
CreateLegacyFieldPredictionFromApiPrediction(
    const AutofillQueryResponse::FormSuggestion::FieldSuggestion::
        FieldPrediction& api_field_prediction) {
  AutofillQueryResponseContents::Field::FieldPrediction legacy_prediction;
  legacy_prediction.set_type(api_field_prediction.type());
  return legacy_prediction;
}

AutofillQueryResponseContents::Field CreateLegacyFieldFromApiField(
    const AutofillQueryResponse::FormSuggestion::FieldSuggestion& api_field) {
  AutofillQueryResponseContents::Field legacy_field;
  legacy_field.set_overall_type_prediction(api_field.primary_type_prediction());
  for (const auto& api_prediction : api_field.predictions()) {
    *legacy_field.add_predictions() =
        CreateLegacyFieldPredictionFromApiPrediction(api_prediction);
  }
  *legacy_field.mutable_password_requirements() =
      api_field.password_requirements();
  return legacy_field;
}

}  // namespace

AutofillPageQueryRequest CreateApiRequestFromLegacyRequest(
    const AutofillQueryContents& legacy_request) {
  AutofillPageQueryRequest api_request;
  *api_request.mutable_experiments() = legacy_request.experiments();
  api_request.set_client_version(legacy_request.client_version());
  for (const auto& legacy_form : legacy_request.form()) {
    *api_request.add_forms() = CreateApiFormFromLegacyForm(legacy_form);
  }
  return api_request;
}

AutofillQueryResponseContents CreateLegacyResponseFromApiResponse(
    const AutofillQueryResponse& api_response) {
  AutofillQueryResponseContents legacy_response;
  for (const auto& api_form : api_response.form_suggestions()) {
    for (const auto& api_field : api_form.field_suggestions()) {
      *legacy_response.add_field() = CreateLegacyFieldFromApiField(api_field);
    }
  }
  return legacy_response;
}

}  // namespace autofill
