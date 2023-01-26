// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <stdint.h>
#include <iterator>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

namespace {

// Returns true, if the prediction is non-experimental and should be used by
// autofill or password manager.
// Note: A `NO_SERVER_DATA` prediction with `SOURCE_UNSPECIFIED` may also be a
// default prediction. We don't need to store it, because its meaning is that
// there is no default prediction.
bool IsDefaultPrediction(const FieldPrediction& prediction) {
  constexpr DenseSet<FieldPrediction::Source, FieldPrediction::Source_MAX>
      default_sources = {FieldPrediction::SOURCE_AUTOFILL_DEFAULT,
                         FieldPrediction::SOURCE_PASSWORDS_DEFAULT,
                         FieldPrediction::SOURCE_OVERRIDE};
  return default_sources.contains(prediction.source());
}

// Compare two field log events of any type to check their log types and
// their attributes related to autofill or editing. If they are the same type
// and their key attributes of the type are the same, we consider |event2| is
// identical to |event1|, we will not add |event2| after |event1| to
// |field_log_events_|.
bool AreCollapsibleLogEvents(const AutofillField::FieldLogEventType& event1,
                             const AutofillField::FieldLogEventType& event2) {
  if (event1.index() != event2.index()) {
    return false;
  }

  static_assert(
      absl::variant_size<AutofillField::FieldLogEventType>() == 8,
      "If you add a new field event type, you need to update this function");

  if (absl::holds_alternative<absl::monostate>(event1)) {
    using E = absl::monostate;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<AskForValuesToFillFieldLogEvent>(event1)) {
    using E = AskForValuesToFillFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<TriggerFillFieldLogEvent>(event1)) {
    using E = TriggerFillFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<FillFieldLogEvent>(event1)) {
    using E = FillFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<TypingFieldLogEvent>(event1)) {
    using E = TypingFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<HeuristicPredictionFieldLogEvent>(event1)) {
    using E = HeuristicPredictionFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<AutocompleteAttributeFieldLogEvent>(event1)) {
    using E = AutocompleteAttributeFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  if (absl::holds_alternative<ServerPredictionFieldLogEvent>(event1)) {
    using E = ServerPredictionFieldLogEvent;
    return AreCollapsible(absl::get<E>(event1), absl::get<E>(event2));
  }

  NOTREACHED();
  return false;
}

}  // namespace

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
  return heuristic_type(GetActivePatternSource());
}

ServerFieldType AutofillField::heuristic_type(PatternSource s) const {
  ServerFieldType type = local_type_predictions_[static_cast<size_t>(s)];
  // `NO_SERVER_DATA` would mean that there is no heuristic type. Client code
  // presumes there is a prediction, therefore we coalesce to `UNKNOWN_TYPE`.
  // Shadow predictions however are not used and we care whether the type is
  // `UNKNOWN_TYPE` or whether we never ran the heuristics.
  return (type > 0 || s != GetActivePatternSource()) ? type : UNKNOWN_TYPE;
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

void AutofillField::set_heuristic_type(PatternSource s, ServerFieldType type) {
  if (type < 0 || type > MAX_VALID_FIELD_TYPE ||
      type == FIELD_WITH_DEFAULT_VALUE) {
    NOTREACHED();
    // This case should not be reachable; but since this has potential
    // implications on data uploaded to the server, better safe than sorry.
    type = UNKNOWN_TYPE;
  }
  local_type_predictions_[static_cast<size_t>(s)] = type;
  if (s == GetActivePatternSource())
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
    std::vector<FieldPrediction> predictions) {
  overall_type_ = AutofillType(NO_SERVER_DATA);
  // Ensures that AutofillField::server_type() is a valid enum value.
  for (auto& prediction : predictions) {
    prediction.set_type(
        ToSafeServerFieldType(prediction.type(), NO_SERVER_DATA));
  }

  server_predictions_.clear();
  experimental_server_predictions_.clear();

  for (auto& prediction : predictions) {
    if (prediction.has_source()) {
      if (prediction.source() == FieldPrediction::SOURCE_UNSPECIFIED)
        // A prediction with `SOURCE_UNSPECIFIED` is one of two things:
        //   1. No prediction for default, a.k.a. `NO_SERVER_DATA`. The absence
        //      of a prediction may not be creditable to a particular prediction
        //      source.
        //   2. An experiment that is missing from the `PredictionSource` enum.
        //      Protobuf corrects unknown values to 0 when parsing.
        // Neither case is actionable.
        continue;
      if (IsDefaultPrediction(prediction)) {
        server_predictions_.push_back(std::move(prediction));
      } else {
        experimental_server_predictions_.push_back(std::move(prediction));
      }
    } else {
      // TODO(crbug.com/1376045): captured tests store old autofill api response
      // recordings without `source` field. We need to maintain the old behavior
      // until these recordings will be migrated.
      server_predictions_.push_back(std::move(prediction));
    }
  }

  if (server_predictions_.empty())
    // Equivalent to a `NO_SERVER_DATA` prediction from `SOURCE_UNSPECIFIED`.
    server_predictions_.emplace_back();

  LOG_IF(ERROR, server_predictions_.size() > 2)
      << "Expected up to 2 default predictions from the Autofill server. "
         "Actual: "
      << server_predictions_.size();
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

  // If the explicit type is cc-exp and either the server or heuristics agree
  // on a 2 vs 4 digit specialization of cc-exp, use that specialization.
  if (html_type_ == HtmlFieldType::kCreditCardExp) {
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
  if (html_type_ == HtmlFieldType::kUnrecognized && !IsCreditCardPrediction() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillFillAndImportFromMoreFields)) {
    return AutofillType(html_type_, html_mode_);
  }

  // If the autocomplete attribute is neither empty or unrecognized, use it
  // unconditionally.
  if (html_type_ != HtmlFieldType::kUnspecified &&
      html_type_ != HtmlFieldType::kUnrecognized) {
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

    // For structured last name tokens the heuristic predictions get precedence
    // over the server predictions.
    believe_server = believe_server && heuristic_type() != NAME_LAST_SECOND &&
                     heuristic_type() != NAME_LAST_FIRST;

    // For structured address tokens the heuristic predictions get precedence
    // over the server predictions.
    believe_server = believe_server &&
                     heuristic_type() != ADDRESS_HOME_STREET_NAME &&
                     heuristic_type() != ADDRESS_HOME_HOUSE_NUMBER;

    // For merchant promo code fields the heuristic predictions get precedence
    // over the server predictions.
    believe_server =
        believe_server && (heuristic_type() != MERCHANT_PROMO_CODE);

    // For international bank account number (IBAN) fields the heuristic
    // predictions get precedence over the server predictions.
    believe_server = believe_server && (heuristic_type() != IBAN_VALUE);

    // The numeric quanity heuristic should get granted precedence over the
    // server prediction since it tries to catch false-positive server
    // predictions.
    believe_server =
        believe_server &&
        !(heuristic_type() == NUMERIC_QUANTITY &&
          server_type() != UNKNOWN_TYPE &&
          base::FeatureList::IsEnabled(
              features::kAutofillGivePrecedenceToNumericQuantitites));

    if (believe_server)
      return AutofillType(server_type());
  }

  return AutofillType(heuristic_type());
}

AutofillType AutofillField::Type() const {
  // Server Overrides are granted precedence unconditionally.
  if (server_type_prediction_is_override() && server_type() != NO_SERVER_DATA)
    return AutofillType(server_type());

  if (overall_type_.GetStorableType() != NO_SERVER_DATA)
    return overall_type_;
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
  ServerFieldType field_type = Type().GetStorableType();
  return IsFillableFieldType(field_type);
}

bool AutofillField::HasPredictionDespiteUnrecognizedAutocompleteAttribute()
    const {
  return html_type_ == HtmlFieldType::kUnrecognized &&
         !IsCreditCardPrediction() &&
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

void AutofillField::AppendLogEventIfNotRepeated(
    const FieldLogEventType& log_event) {
  // TODO(crbug.com/1325851): Consider to use an Overflow event to stop
  // recording log events into |field_log_events_| to save memory when
  // |field_log_events_| reaches certain threshold, e.g. 1000.

  // Disable it for now until we find a selection criterion to select forms to
  // be recorded into UKM.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillLogUKMEventsWithSampleRate)) {
    return;
  }

  if (field_log_events_.empty() ||
      field_log_events_.back().index() != log_event.index() ||
      !AreCollapsibleLogEvents(field_log_events_.back(), log_event)) {
    field_log_events_.push_back(log_event);
  }
}

}  // namespace autofill
