// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/form_field.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/address_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field.h"
#include "components/autofill/core/browser/form_parsing/email_field.h"
#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field.h"
#include "components/autofill/core/browser/form_parsing/name_field.h"
#include "components/autofill/core/browser/form_parsing/phone_field.h"
#include "components/autofill/core/browser/form_parsing/price_field.h"
#include "components/autofill/core/browser/form_parsing/search_field.h"
#include "components/autofill/core/browser/form_parsing/travel_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

// There's an implicit precedence determined by the values assigned here. Email
// is currently the most important followed by Phone, Travel, Address,
// Credit Card, Price, Name, Merchant promo code, and Search.
const float FormField::kBaseEmailParserScore = 1.4f;
const float FormField::kBasePhoneParserScore = 1.3f;
const float FormField::kBaseTravelParserScore = 1.2f;
const float FormField::kBaseAddressParserScore = 1.1f;
const float FormField::kBaseCreditCardParserScore = 1.0f;
const float FormField::kBasePriceParserScore = 0.95f;
const float FormField::kBaseNameParserScore = 0.9f;
const float FormField::kBaseMerchantPromoCodeParserScore = 0.85f;
const float FormField::kBaseSearchParserScore = 0.8f;

// static
FieldCandidatesMap FormField::ParseFormFields(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const LanguageCode& page_language,
    bool is_form_tag,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);
  FieldCandidatesMap field_candidates;

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, processed_fields, &field_candidates,
                      page_language, log_manager);
  const size_t email_count = field_candidates.size();

  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeField::Parse, processed_fields,
                      &field_candidates, page_language, log_manager);
  const size_t promo_code_count = field_candidates.size() - email_count;

  // Phone pass.
  ParseFormFieldsPass(PhoneField::Parse, processed_fields, &field_candidates,
                      page_language, log_manager);

  // Travel pass.
  ParseFormFieldsPass(TravelField::Parse, processed_fields, &field_candidates,
                      page_language, log_manager);

  // Address pass.
  ParseFormFieldsPass(autofill::AddressField::Parse, processed_fields,
                      &field_candidates, page_language, log_manager);

  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, processed_fields,
                      &field_candidates, page_language, log_manager);

  // Price pass.
  ParseFormFieldsPass(PriceField::Parse, processed_fields, &field_candidates,
                      page_language, log_manager);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, processed_fields, &field_candidates,
                      page_language, log_manager);

  // Search pass.
  ParseFormFieldsPass(SearchField::Parse, processed_fields, &field_candidates,
                      page_language, log_manager);

  size_t fillable_fields = 0;
  if (base::FeatureList::IsEnabled(features::kAutofillFixFillableFieldTypes)) {
    for (const auto& candidate : field_candidates) {
      if (IsFillableFieldType(candidate.second.BestHeuristicType()))
        ++fillable_fields;
    }
  } else {
    fillable_fields = field_candidates.size();
  }

  // Do not autofill a form if there aren't enough fields. Otherwise, it is
  // very easy to have false positives. See http://crbug.com/447332
  // For <form> tags, make an exception for email fields, which are commonly
  // the only recognized field on account registration sites. Also make an
  // exception for promo code fields, which are often a single field in its own
  // form.
  if (fillable_fields < kMinRequiredFieldsForHeuristics) {
    if ((is_form_tag && email_count > 0) || promo_code_count > 0) {
      base::EraseIf(field_candidates, [&](const auto& candidate) {
        return !(candidate.second.BestHeuristicType() == EMAIL_ADDRESS ||
                 candidate.second.BestHeuristicType() == MERCHANT_PROMO_CODE);
      });
    } else {
      if (log_manager) {
        LogBuffer table_rows;
        for (const auto& field : fields) {
          table_rows << Tr{} << "Field:" << *field;
        }
        for (const auto& candidate : field_candidates) {
          LogBuffer name;
          name << "Type candidate for frame and renderer ID: "
               << candidate.first;
          LogBuffer description;
          ServerFieldType field_type = candidate.second.BestHeuristicType();
          description << "BestHeuristicType: "
                      << AutofillType::ServerFieldTypeToString(field_type)
                      << ", is fillable: " << IsFillableFieldType(field_type);
          table_rows << Tr{} << std::move(name) << std::move(description);
        }
        log_manager->Log()
            << LoggingScope::kParsing
            << LogMessage::kLocalHeuristicDidNotFindEnoughFillableFields
            << Tag{"table"} << Attrib{"class", "form"} << std::move(table_rows)
            << CTag{"table"};
      }
      field_candidates.clear();
    }
  }

  return field_candidates;
}

FieldCandidatesMap FormField::ParseFormFieldsForPromoCodes(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const LanguageCode& page_language,
    bool is_form_tag,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);
  FieldCandidatesMap field_candidates;

  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeField::Parse, processed_fields,
                      &field_candidates, page_language, log_manager);

  return field_candidates;
}

// static
bool FormField::ParseField(AutofillScanner* scanner,
                           base::StringPiece16 pattern,
                           AutofillField** match,
                           const RegExLogging& logging) {
  return ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, match, logging);
}

bool FormField::ParseField(AutofillScanner* scanner,
                           const std::vector<MatchingPattern>& patterns,
                           AutofillField** match,
                           const RegExLogging& logging) {
  return ParseFieldSpecifics(scanner, patterns, match, logging);
}

bool FormField::ParseField(AutofillScanner* scanner,
                           base::StringPiece16 pattern,
                           const std::vector<MatchingPattern>& patterns,
                           AutofillField** match,
                           const RegExLogging& logging) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillParsingPatternsLanguageDependent) ||
      base::FeatureList::IsEnabled(
          features::kAutofillParsingPatternsNegativeMatching)) {
    return ParseField(scanner, patterns, match, logging);
  } else {
    return ParseField(scanner, pattern, match, logging);
  }
}

bool FormField::ParseFieldSpecifics(AutofillScanner* scanner,
                                    base::StringPiece16 pattern,
                                    int match_field_attributes,
                                    int match_field_input_types,
                                    AutofillField** match,
                                    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(field->form_control_type,
                              match_field_input_types))
    return false;

  return MatchAndAdvance(scanner, pattern, match_field_attributes,
                         match_field_input_types, match, logging);
}

bool FormField::ParseFieldSpecifics(
    AutofillScanner* scanner,
    const std::vector<MatchingPattern>& patterns,
    AutofillField** match,
    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  for (const auto& pattern : patterns) {
    if (!MatchesFormControlType(field->form_control_type,
                                pattern.match_field_input_types)) {
      continue;
    }

    // TODO(crbug.com/1132831): Remove feature check once launched.
    if (base::FeatureList::IsEnabled(
            features::kAutofillParsingPatternsNegativeMatching)) {
      if (!pattern.negative_pattern.empty() &&
          FormField::Match(field, pattern.negative_pattern,
                           pattern.match_field_attributes,
                           pattern.match_field_input_types, logging)) {
        continue;
      }
    }

    if (!pattern.positive_pattern.empty() &&
        MatchAndAdvance(scanner, pattern.positive_pattern,
                        pattern.match_field_attributes,
                        pattern.match_field_input_types, match, logging)) {
      return true;
    }
  }
  return false;
}

// static
bool FormField::ParseFieldSpecifics(AutofillScanner* scanner,
                                    base::StringPiece16 pattern,
                                    int match_type,
                                    AutofillField** match,
                                    const RegExLogging& logging) {
  int match_field_attributes = match_type & 0b11;
  int match_field_types = match_type & ~0b11;

  return ParseFieldSpecifics(scanner, pattern, match_field_attributes,
                             match_field_types, match, logging);
}

bool FormField::ParseFieldSpecifics(
    AutofillScanner* scanner,
    base::StringPiece16 pattern,
    int match_type,
    const std::vector<MatchingPattern>& patterns,
    AutofillField** match,
    const RegExLogging& logging,
    MatchFieldBitmasks match_field_bitmasks) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillParsingPatternsLanguageDependent) ||
      base::FeatureList::IsEnabled(
          features::kAutofillParsingPatternsNegativeMatching)) {
    // TODO(crbug/1142936): This hack is to allow
    // AddressField::ParseNameAndLabelSeparately().
    if (match_field_bitmasks.restrict_attributes != ~0 ||
        match_field_bitmasks.augment_types != 0) {
      std::vector<MatchingPattern> modified_patterns = patterns;
      for (MatchingPattern& mp : modified_patterns) {
        mp.match_field_attributes &= match_field_bitmasks.restrict_attributes;
        mp.match_field_input_types |= match_field_bitmasks.augment_types;
      }
      return ParseFieldSpecifics(scanner, modified_patterns, match, logging);
    }
    return ParseFieldSpecifics(scanner, patterns, match, logging);
  } else {
    return ParseFieldSpecifics(scanner, pattern, match_type, match, logging);
  }
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                AutofillField** match) {
  return ParseFieldSpecifics(scanner, u"^$", MATCH_LABEL | MATCH_ALL_INPUTS,
                             match);
}

// static
void FormField::AddClassification(const AutofillField* field,
                                  ServerFieldType type,
                                  float score,
                                  FieldCandidatesMap* field_candidates) {
  // Several fields are optional.
  if (field == nullptr)
    return;

  FieldCandidates& candidates = (*field_candidates)[field->global_id()];
  candidates.AddFieldCandidate(type, score);
}

// static
std::vector<AutofillField*> FormField::RemoveCheckableFields(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  // Set up a working copy of the fields to be processed.
  std::vector<AutofillField*> processed_fields;
  for (const auto& field : fields) {
    // Ignore checkable fields as they interfere with parsers assuming context.
    // Eg., while parsing address, "Is PO box" checkbox after ADDRESS_LINE1
    // interferes with correctly understanding ADDRESS_LINE2.
    // Ignore fields marked as presentational, unless for 'select' fields (for
    // synthetic fields.)
    if (IsCheckable(field->check_status) ||
        (field->role == FormFieldData::RoleAttribute::kPresentation &&
         field->form_control_type != "select-one")) {
      continue;
    }
    processed_fields.push_back(field.get());
  }
  return processed_fields;
}

bool FormField::MatchAndAdvance(AutofillScanner* scanner,
                                base::StringPiece16 pattern,
                                int match_field_attributes,
                                int match_field_input_types,
                                AutofillField** match,
                                const RegExLogging& logging) {
  AutofillField* field = scanner->Cursor();
  if (FormField::Match(field, pattern, match_field_attributes,
                       match_field_input_types, logging)) {
    if (match)
      *match = field;
    scanner->Advance();
    return true;
  }

  return false;
}

// static
bool FormField::MatchAndAdvance(AutofillScanner* scanner,
                                base::StringPiece16 pattern,
                                int match_type,
                                AutofillField** match,
                                const RegExLogging& logging) {
  int match_field_attributes = match_type & 0b11;
  int match_field_types = match_type & ~0b11;

  return MatchAndAdvance(scanner, pattern, match_field_attributes,
                         match_field_types, match, logging);
}

bool FormField::Match(const AutofillField* field,
                      base::StringPiece16 pattern,
                      int match_field_attributes,
                      int match_field_input_types,
                      const RegExLogging& logging) {
  bool found_match = false;
  base::StringPiece match_type_string;
  base::StringPiece16 value;
  std::u16string match;

  // TODO(crbug/1165780): Remove once shared labels are launched.
  const std::u16string& label =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForParsingWithSharedLabels)
          ? field->parseable_label()
          : field->label;

  const std::u16string& name = field->parseable_name();

  if ((match_field_attributes & MATCH_LABEL) &&
      MatchesPattern(label, pattern, &match)) {
    found_match = true;
    match_type_string = "Match in label";
    value = label;
  } else if ((match_field_attributes & MATCH_NAME) &&
             MatchesPattern(name, pattern, &match)) {
    found_match = true;
    match_type_string = "Match in name";
    value = name;
  }

  if (found_match && logging.log_manager) {
    LogBuffer table_rows;
    table_rows << Tr{} << "Match type:" << match_type_string;
    table_rows << Tr{} << "RegEx:" << logging.regex_name;
    table_rows << Tr{} << "Value: " << HighlightValue(value, match);
    // The matched substring is reported once more as the highlighting is not
    // particularly copy&paste friendly.
    table_rows << Tr{} << "Matched substring: " << match;
    logging.log_manager->Log()
        << LoggingScope::kParsing << LogMessage::kLocalHeuristicRegExMatched
        << Tag{"table"} << std::move(table_rows) << CTag{"table"};
  }

  return found_match;
}

// static
bool FormField::Match(const AutofillField* field,
                      base::StringPiece16 pattern,
                      int match_type,
                      const RegExLogging& logging) {
  int match_field_attributes = match_type & 0b11;
  int match_field_types = match_type & ~0b11;

  return Match(field, pattern, match_field_attributes, match_field_types,
               logging);
}

// static
void FormField::ParseFormFieldsPass(ParseFunction parse,
                                    const std::vector<AutofillField*>& fields,
                                    FieldCandidatesMap* field_candidates,
                                    const LanguageCode& page_language,
                                    LogManager* log_manager) {
  AutofillScanner scanner(fields);
  while (!scanner.IsEnd()) {
    std::unique_ptr<FormField> form_field =
        parse(&scanner, page_language, log_manager);
    if (form_field == nullptr) {
      scanner.Advance();
    } else {
      // Add entries into |field_candidates| for each field type found in
      // |fields|.
      form_field->AddClassifications(field_candidates);
    }
  }
}

bool FormField::MatchesFormControlType(const std::string& type,
                                       int match_type) {
  if ((match_type & MATCH_TEXT) && type == "text")
    return true;

  if ((match_type & MATCH_EMAIL) && type == "email")
    return true;

  if ((match_type & MATCH_TELEPHONE) && type == "tel")
    return true;

  if ((match_type & MATCH_SELECT) && type == "select-one")
    return true;

  if ((match_type & MATCH_TEXT_AREA) && type == "textarea")
    return true;

  if ((match_type & MATCH_PASSWORD) && type == "password")
    return true;

  if ((match_type & MATCH_NUMBER) && type == "number")
    return true;

  if ((match_type & MATCH_SEARCH) && type == "search")
    return true;

  return false;
}

}  // namespace autofill
