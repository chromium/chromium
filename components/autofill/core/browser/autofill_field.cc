// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <stdint.h>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

AutofillField::AutofillField() {
  local_type_predictions_.fill(NO_SERVER_DATA);
}

AutofillField::AutofillField(FieldSignature field_signature) : AutofillField() {
  field_signature_ = field_signature;
}

AutofillField::AutofillField(const FormFieldData& field)
    : FormFieldData(field),
      parseable_name_(field.name),
      parseable_label_(field.label) {
  field_signature_ =
      CalculateFieldSignatureByNameAndType(name, form_control_type);
  local_type_predictions_.fill(NO_SERVER_DATA);
}

AutofillField::~AutofillField() = default;

std::unique_ptr<AutofillField> AutofillField::CreateForPasswordManagerUpload(
    FieldSignature field_signature) {
  std::unique_ptr<AutofillField> field;
  field.reset(new AutofillField(field_signature));
  return field;
}

ServerFieldType AutofillField::heuristic_type() const {
  ServerFieldType type = get_prediction(PredictionSource::kDefaultHeuristics);
  // `NO_SERVER_DATA` would mean that there is no heuristic type. Client code
  // presumes there is a prediction, therefore we coalesce to `UNKNOWN_TYPE`.
  return type > 0 ? type : UNKNOWN_TYPE;
}

ServerFieldType AutofillField::server_type() const {
  return server_predictions_.empty()
             ? NO_SERVER_DATA
             : ToSafeServerFieldType(server_predictions_[0].type(),
                                     NO_SERVER_DATA);
}

bool AutofillField::server_type_prediction_is_override() const {
  return server_predictions_.empty() ? false
                                     : server_predictions_[0].override();
}

void AutofillField::set_heuristic_type(ServerFieldType type) {
  if (type >= 0 && type < MAX_VALID_FIELD_TYPE &&
      type != FIELD_WITH_DEFAULT_VALUE) {
    set_prediction(PredictionSource::kDefaultHeuristics, type);
  } else {
    NOTREACHED();
    // This case should not be reachable; but since this has potential
    // implications on data uploaded to the server, better safe than sorry.
    set_prediction(PredictionSource::kDefaultHeuristics, UNKNOWN_TYPE);
  }
  overall_type_ = AutofillType(NO_SERVER_DATA);
}

void AutofillField::add_possible_types_validities(
    const ServerFieldTypeValidityStateMap& possible_types_validities) {
  for (const auto& possible_type_validity : possible_types_validities) {
    possible_types_validities_[possible_type_validity.first].push_back(
        possible_type_validity.second);
  }
}

void AutofillField::set_server_predictions(
    std::vector<
        AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>
        predictions) {
  server_predictions_ = std::move(predictions);
  overall_type_ = AutofillType(NO_SERVER_DATA);
  // Ensures that AutofillField::server_type() is a valid enum value.
  for (auto& prediction : server_predictions_) {
    prediction.set_type(
        ToSafeServerFieldType(prediction.type(), NO_SERVER_DATA));
  }
}

std::vector<AutofillDataModel::ValidityState>
AutofillField::get_validities_for_possible_type(ServerFieldType type) {
  if (possible_types_validities_.find(type) == possible_types_validities_.end())
    return {AutofillDataModel::UNVALIDATED};
  return possible_types_validities_[type];
}

void AutofillField::SetHtmlType(HtmlFieldType type, HtmlFieldMode mode) {
  html_type_ = type;
  html_mode_ = mode;
  overall_type_ = AutofillType(NO_SERVER_DATA);

  if (type == HTML_TYPE_TEL_LOCAL_PREFIX)
    phone_part_ = PHONE_PREFIX;
  else if (type == HTML_TYPE_TEL_LOCAL_SUFFIX)
    phone_part_ = PHONE_SUFFIX;
  else
    phone_part_ = IGNORED;
}

void AutofillField::SetTypeTo(const AutofillType& type) {
  DCHECK(type.GetStorableType() != NO_SERVER_DATA);
  overall_type_ = type;
}

AutofillType AutofillField::ComputedType() const {
  // If autocomplete=tel/tel-* and server confirms it really is a phone field,
  // we always user the server prediction as html types are not very reliable.
  if ((GroupTypeOfHtmlFieldType(html_type_, html_mode_) ==
           FieldTypeGroup::kPhoneBilling ||
       GroupTypeOfHtmlFieldType(html_type_, html_mode_) ==
           FieldTypeGroup::kPhoneHome) &&
      (GroupTypeOfServerFieldType(server_type()) ==
           FieldTypeGroup::kPhoneBilling ||
       GroupTypeOfServerFieldType(server_type()) ==
           FieldTypeGroup::kPhoneHome)) {
    return AutofillType(server_type());
  }

  // If the explicit type is cc-exp and either the server or heuristics agree on
  // a 2 vs 4 digit specialization of cc-exp, use that specialization.
  if (html_type_ == HTML_TYPE_CREDIT_CARD_EXP) {
    if (server_type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        server_type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
      return AutofillType(server_type());
    }
    if (heuristic_type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        heuristic_type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
      return AutofillType(heuristic_type());
    }
  }

  // If the autocomplete attribute is unrecognized, it is used to effectively
  // return an UNKNOWN_TYPE predition, unless either the heuristic or server
  // prediction suggest that the field is credit-card related, or if the
  // |kAutofillFillAndImportFromMoreFields| feature is enabled.
  if (html_type_ == HTML_TYPE_UNRECOGNIZED && !IsCreditCardPrediction() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillFillAndImportFromMoreFields)) {
    return AutofillType(html_type_, html_mode_);
  }

  // If the autocomplete attribute is neither empty or unrecognized, use it
  // unconditionally.
  if (html_type_ != HTML_TYPE_UNSPECIFIED &&
      html_type_ != HTML_TYPE_UNRECOGNIZED) {
    return AutofillType(html_type_, html_mode_);
  }

  if (server_type() != NO_SERVER_DATA) {
    // Sometimes the server and heuristics disagree on whether a name field
    // should be associated with an address or a credit card. There was a
    // decision to prefer the heuristics in these cases, but it looks like
    // it might be better to fix this server-side.
    // See http://crbug.com/429236 for background.
    bool believe_server = !(server_type() == NAME_FULL &&
                            heuristic_type() == CREDIT_CARD_NAME_FULL) &&
                          !(server_type() == CREDIT_CARD_NAME_FULL &&
                            heuristic_type() == NAME_FULL) &&
                          !(server_type() == NAME_FIRST &&
                            heuristic_type() == CREDIT_CARD_NAME_FIRST) &&
                          !(server_type() == NAME_LAST &&
                            heuristic_type() == CREDIT_CARD_NAME_LAST);

    // Either way, retain a preference for the the CVC heuristic over the
    // server's password predictions (http://crbug.com/469007)
    believe_server =
        believe_server && !(AutofillType(server_type()).group() ==
                                FieldTypeGroup::kPasswordField &&
                            heuristic_type() == CREDIT_CARD_VERIFICATION_CODE);

    // For new name tokens the heuristic predictions get precedence over the
    // server predictions.
    // TODO(crbug.com/1098943): Remove feature check once launched.
    believe_server =
        believe_server &&
        !(base::FeatureList::IsEnabled(
              features::kAutofillEnableSupportForMoreStructureInNames) &&
          (heuristic_type() == NAME_LAST_SECOND ||
           heuristic_type() == NAME_LAST_FIRST));

    // For new address tokens the heuristic predictions get precedence over the
    // server predictions.
    // TODO(crbug.com/1098943): Remove feature check once launched.
    believe_server =
        believe_server &&
        !(base::FeatureList::IsEnabled(
              features::kAutofillEnableSupportForMoreStructureInAddresses) &&
          (heuristic_type() == ADDRESS_HOME_STREET_NAME ||
           heuristic_type() == ADDRESS_HOME_HOUSE_NUMBER));

    believe_server =
        believe_server && !(heuristic_type() == MERCHANT_PROMO_CODE);

    if (believe_server)
      return AutofillType(server_type());
  }

  return AutofillType(heuristic_type());
}

AutofillType AutofillField::Type() const {
  // If the corresponding feature is enabled, server predictions that are an
  // override are granted precedence unconditionally.
  if (base::FeatureList::IsEnabled(
          features::kAutofillServerTypeTakesPrecedence) &&
      server_type_prediction_is_override() && server_type() != NO_SERVER_DATA) {
    return AutofillType(server_type());
  }

  if (overall_type_.GetStorableType() != NO_SERVER_DATA) {
    return overall_type_;
  }
  return ComputedType();
}

bool AutofillField::IsEmpty() const {
  return value.empty();
}

FieldSignature AutofillField::GetFieldSignature() const {
  return field_signature_
             ? *field_signature_
             : CalculateFieldSignatureByNameAndType(name, form_control_type);
}

std::string AutofillField::FieldSignatureAsStr() const {
  return base::NumberToString(GetFieldSignature().value());
}

bool AutofillField::IsFieldFillable() const {
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFillableFieldTypes))
    return !Type().IsUnknown();

  ServerFieldType field_type = Type().GetStorableType();
  return IsFillableFieldType(field_type);
}

bool AutofillField::ShouldSuppressPromptDueToUnrecognizedAutocompleteAttribute()
    const {
  return html_type_ == HTML_TYPE_UNRECOGNIZED && !IsCreditCardPrediction() &&
         base::FeatureList::IsEnabled(
             features::kAutofillFillAndImportFromMoreFields);
}

void AutofillField::SetPasswordRequirements(PasswordRequirementsSpec spec) {
  password_requirements_ = std::move(spec);
}

void AutofillField::NormalizePossibleTypesValidities() {
  for (auto possible_type : possible_types_) {
    if (possible_types_validities_[possible_type].empty()) {
      possible_types_validities_[possible_type].push_back(
          AutofillDataModel::UNVALIDATED);
    }
  }
}

bool AutofillField::IsCreditCardPrediction() const {
  return AutofillType(server_type()).group() == FieldTypeGroup::kCreditCard ||
         AutofillType(heuristic_type()).group() == FieldTypeGroup::kCreditCard;
}

}  // namespace autofill
