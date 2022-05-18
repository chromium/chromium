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
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
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

namespace {

constexpr bool IsEmpty(const char16_t* s) {
  return s == nullptr || s[0] == '\0';
}

}  // namespace

// static
FieldCandidatesMap FormField::ParseFormFields(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const LanguageCode& page_language,
    bool is_form_tag,
    PatternSource pattern_source,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);
  FieldCandidatesMap field_candidates;

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);
  const size_t email_count = field_candidates.size();

  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeField::Parse, processed_fields,
                      &field_candidates, page_language, pattern_source,
                      log_manager);
  const size_t promo_code_count = field_candidates.size() - email_count;

  // Phone pass.
  ParseFormFieldsPass(PhoneField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);

  // Travel pass.
  ParseFormFieldsPass(TravelField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);

  // Address pass.
  ParseFormFieldsPass(AddressField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);

  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, processed_fields,
                      &field_candidates, page_language, pattern_source,
                      log_manager);

  // Price pass.
  ParseFormFieldsPass(PriceField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);

  // Search pass.
  ParseFormFieldsPass(SearchField::Parse, processed_fields, &field_candidates,
                      page_language, pattern_source, log_manager);

  size_t fillable_fields = 0;
  if (base::FeatureList::IsEnabled(features::kAutofillFixFillableFieldTypes)) {
    for (const auto& [field_id, candidates] : field_candidates) {
      if (IsFillableFieldType(candidates.BestHeuristicType()))
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
        for (const auto& [field_id, candidates] : field_candidates) {
          LogBuffer name;
          name << "Type candidate for frame and renderer ID: " << field_id;
          LogBuffer description;
          ServerFieldType field_type = candidates.BestHeuristicType();
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
    PatternSource pattern_source,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);
  FieldCandidatesMap field_candidates;

  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeField::Parse, processed_fields,
                      &field_candidates, page_language, pattern_source,
                      log_manager);

  return field_candidates;
}

// static
bool FormField::ParseField(AutofillScanner* scanner,
                           base::StringPiece16 pattern,
                           base::span<const MatchPatternRef> patterns,
                           AutofillField** match,
                           const RegExLogging& logging) {
  return ParseFieldSpecifics(scanner, pattern, kDefaultMatchParams, patterns,
                             match, logging);
}

// static
bool FormField::ParseFieldSpecificsWithLegacyPattern(
    AutofillScanner* scanner,
    base::StringPiece16 pattern,
    MatchParams match_type,
    AutofillField** match,
    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(field->form_control_type,
                              match_type.field_types)) {
    return false;
  }

  return MatchAndAdvance(scanner, pattern, match_type, match, logging);
}

// static
bool FormField::ParseFieldSpecificsWithNewPatterns(
    AutofillScanner* scanner,
    base::span<const MatchPatternRef> patterns,
    AutofillField** match,
    const RegExLogging& logging,
    MatchingPattern (*projection)(const MatchingPattern&)) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  for (MatchPatternRef pattern_ref : patterns) {
    MatchingPattern pattern =
        projection ? (*projection)(*pattern_ref) : *pattern_ref;
    if (!MatchesFormControlType(field->form_control_type,
                                pattern.match_field_input_types)) {
      continue;
    }

    // For each of the two match field attributes, kName and kLabel,
    // that are active for the current pattern, test if it matches the negative
    // pattern. If yes, remove it from the attributes that are considered for
    // positive matching.
    MatchParams match_type(pattern.match_field_attributes,
                           pattern.match_field_input_types);

    if (!IsEmpty(pattern.negative_pattern)) {
      for (MatchAttribute attribute : pattern.match_field_attributes) {
        if (FormField::Match(field, pattern.negative_pattern,
                             MatchParams({attribute}, match_type.field_types),
                             logging)) {
          match_type.attributes.erase(attribute);
        }
      }
    }

    if (match_type.attributes.empty())
      continue;

    // Apply the positive matching against all remaining match field attributes.
    if (!IsEmpty(pattern.positive_pattern) &&
        MatchAndAdvance(scanner, pattern.positive_pattern, match_type, match,
                        logging)) {
      return true;
    }
  }
  return false;
}

bool FormField::ParseFieldSpecifics(
    AutofillScanner* scanner,
    base::StringPiece16 pattern,
    const MatchParams& match_type,
    base::span<const MatchPatternRef> patterns,
    AutofillField** match,
    const RegExLogging& logging,
    MatchingPattern (*projection)(const MatchingPattern&)) {
  return base::FeatureList::IsEnabled(features::kAutofillParsingPatternProvider)
             ? ParseFieldSpecificsWithNewPatterns(scanner, patterns, match,
                                                  logging, projection)
             : ParseFieldSpecificsWithLegacyPattern(scanner, pattern,
                                                    match_type, match, logging);
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                AutofillField** match) {
  return ParseFieldSpecificsWithLegacyPattern(
      scanner, u"^$",
      MatchParams({MatchAttribute::kLabel}, kAllMatchFieldTypes), match,
      /*logging=*/{});
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

// static
bool FormField::MatchAndAdvance(AutofillScanner* scanner,
                                base::StringPiece16 pattern,
                                MatchParams match_type,
                                AutofillField** match,
                                const RegExLogging& logging) {
  AutofillField* field = scanner->Cursor();
  if (FormField::Match(field, pattern, match_type, logging)) {
    if (match)
      *match = field;
    scanner->Advance();
    return true;
  }

  return false;
}

bool FormField::Match(const AutofillField* field,
                      base::StringPiece16 pattern,
                      MatchParams match_type,
                      const RegExLogging& logging) {
  bool found_match = false;
  base::StringPiece match_type_string;
  base::StringPiece16 value;
  std::vector<std::u16string> matches;
  std::vector<std::u16string>* capture_destination =
      logging.log_manager ? &matches : nullptr;

  // TODO(crbug/1165780): Remove once shared labels are launched.
  const std::u16string& label =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForParsingWithSharedLabels)
          ? field->parseable_label()
          : field->label;

  const std::u16string& name = field->parseable_name();

  const bool match_label =
      match_type.attributes.contains(MatchAttribute::kLabel);
  if (match_label && MatchesPattern(label, pattern, capture_destination)) {
    found_match = true;
    match_type_string = "Match in label";
    value = label;
  } else if (match_type.attributes.contains(MatchAttribute::kName) &&
             MatchesPattern(name, pattern, capture_destination)) {
    found_match = true;
    match_type_string = "Match in name";
    value = name;
  } else if (match_label &&
             base::FeatureList::IsEnabled(
                 features::kAutofillConsiderPlaceholderForParsing) &&
             MatchesPattern(field->placeholder, pattern, capture_destination)) {
    // TODO(crbug.com/1317961): The label and placeholder cases should logically
    // be grouped together. Placeholder is currently last, because for the finch
    // study we want the group assignment to happen as late as possible.
    // Reorder once the change is rolled out.
    found_match = true;
    match_type_string = "Match in placeholder";
    value = field->placeholder;
  }

  if (found_match && logging.log_manager) {
    LogBuffer table_rows;
    table_rows << Tr{} << "Match type:" << match_type_string;
    table_rows << Tr{} << "RegEx:" << logging.regex_name;
    table_rows << Tr{} << "Value: " << HighlightValue(value, matches[0]);
    // The matched substring is reported once more as the highlighting is not
    // particularly copy&paste friendly.
    table_rows << Tr{} << "Matched substring: " << matches[0];
    logging.log_manager->Log()
        << LoggingScope::kParsing << LogMessage::kLocalHeuristicRegExMatched
        << Tag{"table"} << std::move(table_rows) << CTag{"table"};
  }

  return found_match;
}

// static
void FormField::ParseFormFieldsPass(ParseFunction parse,
                                    const std::vector<AutofillField*>& fields,
                                    FieldCandidatesMap* field_candidates,
                                    const LanguageCode& page_language,
                                    PatternSource pattern_source,
                                    LogManager* log_manager) {
  AutofillScanner scanner(fields);
  while (!scanner.IsEnd()) {
    std::unique_ptr<FormField> form_field =
        parse(&scanner, page_language, pattern_source, log_manager);
    if (form_field == nullptr) {
      scanner.Advance();
    } else {
      // Add entries into |field_candidates| for each field type found in
      // |fields|.
      form_field->AddClassifications(field_candidates);
    }
  }
}

// static
bool FormField::MatchesFormControlType(base::StringPiece type,
                                       DenseSet<MatchFieldType> match_type) {
  if (match_type.contains(MatchFieldType::kText) && type == "text")
    return true;

  if (match_type.contains(MatchFieldType::kEmail) && type == "email")
    return true;

  if (match_type.contains(MatchFieldType::kTelephone) && type == "tel")
    return true;

  if (match_type.contains(MatchFieldType::kSelect) && type == "select-one")
    return true;

  if (match_type.contains(MatchFieldType::kTextArea) && type == "textarea")
    return true;

  if (match_type.contains(MatchFieldType::kPassword) && type == "password")
    return true;

  if (match_type.contains(MatchFieldType::kNumber) && type == "number")
    return true;

  if (match_type.contains(MatchFieldType::kSearch) && type == "search")
    return true;

  return false;
}

}  // namespace autofill
