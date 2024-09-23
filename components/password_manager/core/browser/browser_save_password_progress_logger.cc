// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_printer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/votes_uploader.h"

using autofill::AutofillType;
using autofill::AutofillUploadContents;
using autofill::FieldGlobalId;
using autofill::FieldPropertiesFlags;
using autofill::FieldType;
using autofill::FieldTypeToStringView;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormStructure;
using base::NumberToString;

namespace password_manager {

namespace {

std::string GenerationTypeToString(
    AutofillUploadContents::Field::PasswordGenerationType type) {
  switch (type) {
    case AutofillUploadContents::Field::NO_GENERATION:
      return std::string();
    case AutofillUploadContents::Field::
        AUTOMATICALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM:
      return "Generation on sign-up";
    case AutofillUploadContents::Field::
        AUTOMATICALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM:
      return "Generation on change password";
    case AutofillUploadContents::Field::
        MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM:
      return "Manual generation on sign-up";
    case AutofillUploadContents::Field::
        MANUALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM:
      return "Manual generation on change password";
    case AutofillUploadContents::Field::IGNORED_GENERATION_POPUP:
      return "Generation ignored";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::string();
}

std::string VoteTypeToString(
    AutofillUploadContents::Field::VoteType vote_type) {
  switch (vote_type) {
    case AutofillUploadContents::Field::NO_INFORMATION:
      return "No information";
    case AutofillUploadContents::Field::CREDENTIALS_REUSED:
      return "Credentials reused";
    case AutofillUploadContents::Field::USERNAME_OVERWRITTEN:
      return "Username overwritten";
    case AutofillUploadContents::Field::USERNAME_EDITED:
      return "Username edited";
    case AutofillUploadContents::Field::BASE_HEURISTIC:
      return "Base heuristic";
    case AutofillUploadContents::Field::HTML_CLASSIFIER:
      return "HTML classifier";
    case AutofillUploadContents::Field::FIRST_USE:
      return "First use";
  }
}

std::string FormSignatureToDebugString(autofill::FormSignature form_signature) {
  return base::StrCat(
      {NumberToString(form_signature.value()), " - ",
       NumberToString(
           PasswordFormMetricsRecorder::HashFormSignature(form_signature))});
}

BrowserSavePasswordProgressLogger::StringID FormSchemeToStringID(
    PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::Scheme::kHtml:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_HTML;
    case PasswordForm::Scheme::kBasic:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_BASIC;
    case PasswordForm::Scheme::kDigest:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_DIGEST;
    case PasswordForm::Scheme::kOther:
      return BrowserSavePasswordProgressLogger::STRING_OTHER;
    case PasswordForm::Scheme::kUsernameOnly:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_USERNAME_ONLY;
  }
  return BrowserSavePasswordProgressLogger::STRING_INVALID;
}

// Returns a log string describing `field`.
std::string GetFormFieldDataWithPropertiesMaskLogString(
    const FormFieldData& field) {
  std::string field_info =
      autofill::SavePasswordProgressLogger::GetFormFieldDataLogString(field);

  if (field.properties_mask()) {
    field_info += ", properties=";
    field_info += (field.properties_mask() & FieldPropertiesFlags::kUserTyped)
                      ? "T"
                      : "_";
    field_info +=
        (field.properties_mask() & FieldPropertiesFlags::kAutofilledOnPageLoad)
            ? "Ap"
            : "__";
    field_info += (field.properties_mask() &
                   FieldPropertiesFlags::kAutofilledOnUserTrigger)
                      ? "Au"
                      : "__";
    field_info +=
        (field.properties_mask() & FieldPropertiesFlags::kHadFocus) ? "F" : "_";
    field_info += (field.properties_mask() & FieldPropertiesFlags::kKnownValue)
                      ? "K"
                      : "_";
  }

  return field_info;
}

// Returns a log string describing the fields and their `ServerPrediction`s
// contained in `form`.
std::string GetFormDataFieldsAndPredictionsLogString(
    const FormData& form,
    const base::flat_map<autofill::FieldGlobalId,
                         AutofillType::ServerPrediction>& predictions) {
  std::string result;
  result += BrowserSavePasswordProgressLogger::GetStringFromID(
                BrowserSavePasswordProgressLogger::STRING_FIELDS) +
            ": " + "\n";
  for (const FormFieldData& field : form.fields()) {
    std::string field_info = GetFormFieldDataWithPropertiesMaskLogString(field);

    if (!predictions.contains(field.global_id())) {
      result += field_info + "\n";
      continue;
    }
    const AutofillType::ServerPrediction& prediction =
        predictions.at(field.global_id());

    if (prediction.server_type() != autofill::NO_SERVER_DATA) {
      base::StrAppend(
          &field_info,
          {", Server Type= ", FieldTypeToStringView(prediction.server_type())});

      std::vector<std::string> all_predictions;
      for (const auto& p : prediction.server_predictions) {
        all_predictions.emplace_back(
            FieldTypeToStringView(static_cast<FieldType>(p.type())));
      }

      base::StrAppend(&field_info,
                      {", All Server Predictions= [",
                       base::JoinString(all_predictions, ", "), "]"});
    }

    result += field_info + "\n";
  }

  return result;
}

}  // namespace

BrowserSavePasswordProgressLogger::BrowserSavePasswordProgressLogger(
    autofill::LogManager* log_manager)
    : log_manager_(log_manager) {
  DCHECK(log_manager_);
}

BrowserSavePasswordProgressLogger::~BrowserSavePasswordProgressLogger() =
    default;

void BrowserSavePasswordProgressLogger::LogFormDataWithServerPredictions(
    StringID label,
    const autofill::FormData& form,
    const base::flat_map<autofill::FieldGlobalId,
                         AutofillType::ServerPrediction>& predictions) {
  std::string message = GetStringFromID(label) + ": {\n";
  message +=
      GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
      FormSignatureToDebugString(autofill::CalculateFormSignature(form)) + "\n";
  message += GetStringFromID(STRING_ALTERNATIVE_FORM_SIGNATURE) + ": " +
             FormSignatureToDebugString(
                 autofill::CalculateAlternativeFormSignature(form)) +
             "\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(form.url()) + "\n";
  message +=
      GetStringFromID(STRING_ACTION) + ": " + ScrubURL(form.action()) + "\n";
  message += GetFormDataFieldsAndPredictionsLogString(form, predictions);
  message += "}";
  SendLog(message);
}

void BrowserSavePasswordProgressLogger::LogFormStructure(
    StringID label,
    const FormStructure& form_structure,
    std::optional<PasswordAttributesMetadata> password_attributes) {
  std::string message = GetStringFromID(label) + ": {\n";
  message += GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
             FormSignatureToDebugString(form_structure.form_signature()) + "\n";
  message +=
      GetStringFromID(STRING_ALTERNATIVE_FORM_SIGNATURE) + ": " +
      FormSignatureToDebugString(form_structure.alternative_form_signature()) +
      "\n";
  message += GetStringFromID(STRING_ORIGIN) + ": " +
             ScrubURL(form_structure.source_url()) + "\n";
  message += GetStringFromID(STRING_ACTION) + ": " +
             ScrubURL(form_structure.target_url()) + "\n";
  message += FormStructureToFieldsLogString(form_structure);
  message += VotesPasswordAttributesLogString(password_attributes);
  message += "}";
  SendLog(message);
}

void BrowserSavePasswordProgressLogger::LogSuccessiveOrigins(
    StringID label,
    const GURL& old_origin,
    const GURL& new_origin) {
  std::string message = GetStringFromID(label) + ": {\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(old_origin) + "\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(new_origin) + "\n";
  message += "}";
  SendLog(message);
}

std::string BrowserSavePasswordProgressLogger::VotesPasswordAttributesLogString(
    std::optional<PasswordAttributesMetadata> password_attributes) {
  if (!password_attributes.has_value()) {
    return std::string();
  }
  const std::pair<PasswordAttribute, bool> attribute_vote =
      password_attributes->password_attributes_vote;
  std::string message;
  const auto [attribute, attribute_value] = attribute_vote;

  switch (attribute) {
    case PasswordAttribute::kHasLetter:
      message += BinaryPasswordAttributeLogString(
          STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_LETTER, attribute_value);
      break;

    case PasswordAttribute::kHasSpecialSymbol:
      message += BinaryPasswordAttributeLogString(
          STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIAL_SYMBOL,
          attribute_value);
      if (attribute_value) {
        std::string voted_symbol(
            1, static_cast<char>(password_attributes->password_symbol_vote));
        message += PasswordAttributeLogString(
            STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIFIC_SPECIAL_SYMBOL,
            voted_symbol);
      }
      break;

    case PasswordAttribute::kPasswordAttributesCount:
      break;
  }
  std::string password_length =
      base::NumberToString(password_attributes->password_length_vote);
  message += PasswordAttributeLogString(
      STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_PASSWORD_LENGTH, password_length);

  return message;
}

// static
std::string BrowserSavePasswordProgressLogger::FormStructureToFieldsLogString(
    const FormStructure& form_structure) {
  std::string result;
  result += GetStringFromID(STRING_FIELDS) + ": " + "\n";
  for (const auto& field : form_structure) {
    std::string field_info =
        GetFormFieldDataWithPropertiesMaskLogString(*field);

    if (field->server_type() != autofill::NO_SERVER_DATA) {
      base::StrAppend(
          &field_info,
          {", Server Type: ", FieldTypeToStringView(field->server_type())});

      std::vector<std::string> all_predictions;
      for (const auto& p : field->server_predictions()) {
        all_predictions.emplace_back(
            FieldTypeToStringView(static_cast<FieldType>(p.type())));
      }

      base::StrAppend(&field_info,
                      {", All Server Predictions: [",
                       base::JoinString(all_predictions, ", "), "]"});
    }

    for (FieldType type : field->possible_types()) {
      base::StrAppend(&field_info, {", VOTE: ", FieldTypeToStringView(type)});
    }

    if (field->vote_type()) {
      field_info += ", vote_type=" + VoteTypeToString(field->vote_type());
    }

    if (field->initial_value_hash().has_value()) {
      field_info += ", initial value hash=";
      field_info += NumberToString(field->initial_value_hash().value());
    }

    std::string generation = GenerationTypeToString(field->generation_type());
    if (!generation.empty()) {
      field_info += ", GENERATION_EVENT: " + generation;
    }

    if (field->generated_password_changed()) {
      field_info += ", generated password changed";
    }

    if (field->password_requirements()) {
      std::ostringstream s;
      s << *field->password_requirements();
      base::StrAppend(&field_info, {", PASSWORD_REQUIREMENTS: ", s.str()});
    }

    result += field_info + "\n";
  }

  return result;
}

void BrowserSavePasswordProgressLogger::LogString(StringID label,
                                                  const std::string& s) {
  LogValue(label, base::Value(s));
}

void BrowserSavePasswordProgressLogger::LogSuccessfulSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  std::ostringstream submission_event_string_stream;
  submission_event_string_stream << event;
  std::string message =
      GetStringFromID(STRING_SUCCESSFUL_SUBMISSION_INDICATOR_EVENT) + ": " +
      submission_event_string_stream.str();
  SendLog(message);
}

void BrowserSavePasswordProgressLogger::LogPasswordForm(
    BrowserSavePasswordProgressLogger::StringID label,
    const PasswordForm& form) {
  base::Value::Dict log;
  log.Set(GetStringFromID(STRING_SCHEME_MESSAGE),
          GetStringFromID(FormSchemeToStringID(form.scheme)));
  log.Set(GetStringFromID(STRING_SCHEME_MESSAGE),
          GetStringFromID(FormSchemeToStringID(form.scheme)));
  log.Set(GetStringFromID(STRING_SIGNON_REALM),
          ScrubURL(GURL(form.signon_realm)));
  log.Set(GetStringFromID(STRING_ORIGIN), ScrubURL(form.url));
  log.Set(GetStringFromID(STRING_ACTION), ScrubURL(form.action));
  log.Set(GetStringFromID(STRING_USERNAME_ELEMENT),
          ScrubElementID(form.username_element));
  log.Set(GetStringFromID(STRING_USERNAME_ELEMENT_RENDERER_ID),
          NumberToString(form.username_element_renderer_id.value()));
  log.Set(GetStringFromID(STRING_PASSWORD_ELEMENT),
          ScrubElementID(form.password_element));
  log.Set(GetStringFromID(STRING_PASSWORD_ELEMENT_RENDERER_ID),
          NumberToString(form.password_element_renderer_id.value()));
  log.Set(GetStringFromID(STRING_NEW_PASSWORD_ELEMENT),
          ScrubElementID(form.new_password_element));
  log.Set(GetStringFromID(STRING_NEW_PASSWORD_ELEMENT_RENDERER_ID),
          NumberToString(form.new_password_element_renderer_id.value()));
  if (!form.confirmation_password_element.empty()) {
    log.Set(GetStringFromID(STRING_CONFIRMATION_PASSWORD_ELEMENT),
            ScrubElementID(form.confirmation_password_element));
    log.Set(
        GetStringFromID(STRING_CONFIRMATION_PASSWORD_ELEMENT_RENDERER_ID),
        NumberToString(form.confirmation_password_element_renderer_id.value()));
  }
  log.Set(GetStringFromID(STRING_PASSWORD_GENERATED),
          form.type == PasswordForm::Type::kGenerated);
  log.Set(GetStringFromID(STRING_TIMES_USED), form.times_used_in_html_form);
  LogValue(label, base::Value(std::move(log)));
}

void BrowserSavePasswordProgressLogger::LogPasswordRequirements(
    const GURL& origin,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    const autofill::PasswordRequirementsSpec& spec) {
  std::ostringstream s;
  s << "Joint password requirements: {\n"
    << GetStringFromID(STRING_ORIGIN) << ": " << ScrubURL(origin) << "\n"
    << GetStringFromID(STRING_FORM_SIGNATURE) << ": "
    << FormSignatureToDebugString(form_signature) << "\n"
    << "Field signature: " << NumberToString(field_signature.value()) << "\n"
    << "Requirements:" << spec << "\n"
    << "}";
  SendLog(s.str());
}

void BrowserSavePasswordProgressLogger::SendLog(const std::string& log) {
  LOG_AF(*log_manager_) << autofill::Tag{"div"}
                        << autofill::Attrib{"class", "preserve-white-space"}
                        << log << autofill::CTag{"div"};
}

// static
std::string BrowserSavePasswordProgressLogger::PasswordAttributeLogString(
    StringID string_id,
    const std::string& attribute_value) {
  return GetStringFromID(string_id) + ": " + attribute_value + "\n";
}

// static
std::string BrowserSavePasswordProgressLogger::BinaryPasswordAttributeLogString(
    StringID string_id,
    bool attribute_value) {
  return PasswordAttributeLogString(string_id,
                                    (attribute_value ? "yes" : "no"));
}

}  // namespace password_manager
