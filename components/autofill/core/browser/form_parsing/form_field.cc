// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/form_field.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/birthdate_field.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field.h"
#include "components/autofill/core/browser/form_parsing/email_field.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_parsing/iban_field.h"
#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field.h"
#include "components/autofill/core/browser/form_parsing/name_field.h"
#include "components/autofill/core/browser/form_parsing/numeric_quantity_field.h"
#include "components/autofill/core/browser/form_parsing/phone_field.h"
#include "components/autofill/core/browser/form_parsing/price_field.h"
#include "components/autofill/core/browser/form_parsing/search_field.h"
#include "components/autofill/core/browser/form_parsing/standalone_cvc_field.h"
#include "components/autofill/core/browser/form_parsing/travel_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

namespace {

constexpr char16_t kEmptyLabelRegex[] = u"^$";

constexpr bool IsEmpty(const char16_t* s) {
  return s == nullptr || s[0] == '\0';
}

}  // namespace

// static
bool FormField::MatchesRegexWithCache(base::StringPiece16 input,
                                      base::StringPiece16 pattern,
                                      std::vector<std::u16string>* groups) {
  // TODO(crbug.com/1309848): If ParseForm() is called from the same thread,
  // use a thread-unsafe parser.
  static base::NoDestructor<AutofillRegexCache> cache(ThreadSafe(true));
  const icu::RegexPattern* regex_pattern = cache->GetRegexPattern(pattern);
  return autofill::MatchesRegex(input, *regex_pattern, groups);
}

// static
void FormField::ParseFormFields(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const LanguageCode& page_language,
    bool is_form_tag,
    PatternSource pattern_source,
    FieldCandidatesMap& field_candidates,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);
  const size_t email_count = field_candidates.size();

  // Single fields pass.
  ParseSingleFieldForms(fields, page_language, is_form_tag, pattern_source,
                        field_candidates, log_manager);
  const size_t fillable_single_fields = field_candidates.size() - email_count;

  // Phone pass.
  ParseFormFieldsPass(PhoneField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);

  // Travel pass.
  ParseFormFieldsPass(TravelField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);

  // Address pass.
  ParseFormFieldsPass(AddressField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);

  // Birthdate pass.
  if (base::FeatureList::IsEnabled(features::kAutofillEnableBirthdateParsing)) {
    ParseFormFieldsPass(BirthdateField::Parse, processed_fields,
                        field_candidates, page_language, pattern_source,
                        log_manager);
  }

  // Numeric quantity pass.
  ParseFormFieldsPass(NumericQuantityField::Parse, processed_fields,
                      field_candidates, page_language, pattern_source,
                      log_manager);

  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, processed_fields,
                      field_candidates, page_language, pattern_source,
                      log_manager);

  // Price pass.
  ParseFormFieldsPass(PriceField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);

  // Search pass.
  ParseFormFieldsPass(SearchField::Parse, processed_fields, field_candidates,
                      page_language, pattern_source, log_manager);

  // Deduce `field_candidates` for the `processed_fields` by parsing their
  // `parsable_name()` as an autocomplete attribute.
  if (base::FeatureList::IsEnabled(
          features::kAutofillParseNameAsAutocompleteType)) {
    ParseUsingAutocompleteAttributes(processed_fields, field_candidates);
  }

  size_t fillable_distinct_field_types = 0;
  // Set to count distinct field types.
  ServerFieldTypeSet heuristic_types;
  for (const auto& [field_id, candidates] : field_candidates) {
    if (IsFillableFieldType(candidates.BestHeuristicType())) {
      ++fillable_distinct_field_types;
      heuristic_types.insert(candidates.BestHeuristicType());
    }
  }

  // We consider the number of distinct fillable field types, not the number of
  // distinct fillable fields, to determine whether local heuristics should be
  // applied. This reduces false positives by counting similar fields only once.
  // "Fillable" refers to the field type, not whether a specific field is
  // visible and editable by the user.
  fillable_distinct_field_types = heuristic_types.size();

  // Do not autofill a form if there aren't enough fields. Otherwise, it is
  // very easy to have false positives. See http://crbug.com/447332
  // For <form> tags, make an exception for email fields, which are commonly
  // the only recognized field on account registration sites. Also make an
  // exception for single-field Autofillable types, even when the form contains
  // less than kMinRequiredFieldsForHeuristics fields in its form signature.
  if (fillable_distinct_field_types < kMinRequiredFieldsForHeuristics) {
    if ((is_form_tag && email_count > 0) || fillable_single_fields > 0) {
      base::EraseIf(field_candidates, [&](const auto& candidate) {
        return !(candidate.second.BestHeuristicType() == EMAIL_ADDRESS ||
                 FormField::IsSingleFieldParseableType(
                     candidate.second.BestHeuristicType()));
      });
    } else {
      LogBuffer table_rows(IsLoggingActive(log_manager));
      for (const auto& field : fields)
        LOG_AF(table_rows) << Tr{} << "Field:" << *field;
      for (const auto& [field_id, candidates] : field_candidates) {
        LogBuffer name(IsLoggingActive(log_manager));
        name << "Type candidate for frame and renderer ID: " << field_id;
        LogBuffer description(IsLoggingActive(log_manager));
        LOG_AF(description)
            << "BestHeuristicType: "
            << AutofillType::ServerFieldTypeToString(
                   candidates.BestHeuristicType())
            << ", is fillable: "
            << IsFillableFieldType(candidates.BestHeuristicType());
        LOG_AF(table_rows) << Tr{} << std::move(name) << std::move(description);
      }
      LOG_AF(log_manager)
          << LoggingScope::kParsing
          << LogMessage::kLocalHeuristicDidNotFindEnoughFillableFields
          << Tag{"table"} << Attrib{"class", "form"} << std::move(table_rows)
          << CTag{"table"};
      field_candidates.clear();
    }
  }
}

void FormField::ParseSingleFieldForms(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const LanguageCode& page_language,
    bool is_form_tag,
    PatternSource pattern_source,
    FieldCandidatesMap& field_candidates,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);

  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeField::Parse, processed_fields,
                      field_candidates, page_language, pattern_source,
                      log_manager);

  // IBAN pass.
  if (base::FeatureList::IsEnabled(features::kAutofillParseIBANFields)) {
    ParseFormFieldsPass(IBANField::Parse, processed_fields, field_candidates,
                        page_language, pattern_source, log_manager);
  }
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

// static
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
bool FormField::ParseInAnyOrder(
    AutofillScanner* scanner,
    std::vector<std::pair<AutofillField**, base::RepeatingCallback<bool()>>>
        fields_and_parsers) {
  if (scanner->IsEnd())
    return fields_and_parsers.empty();
  auto original_pos = scanner->SaveCursor();
  // The implementation tries matching every permutation `p` of parsers with the
  // scanners fields. While this has a terrible runtime for general n, the only
  // planned use cases are dates (2 or 3 components).
  // If necessary, bipartite matching could be used for general n.
  DCHECK(fields_and_parsers.size() <= 3);
  std::vector<int> p(fields_and_parsers.size());
  std::iota(p.begin(), p.end(), 0);
  do {
    bool matches = true;
    for (int i : p) {
      const auto& [field, parser] = fields_and_parsers[i];
      if (!scanner->IsEnd() && parser.Run()) {
        *field = scanner->Cursor();
        scanner->Advance();
      } else {
        matches = false;
        break;
      }
    }
    if (matches)
      return true;
    scanner->RewindTo(original_pos);
  } while (std::next_permutation(p.begin(), p.end()));
  for (const auto& [field, _] : fields_and_parsers)
    *field = nullptr;
  return false;
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                AutofillField** match) {
  return ParseFieldSpecificsWithLegacyPattern(
      scanner, kEmptyLabelRegex,
      MatchParams({MatchAttribute::kLabel}, kAllMatchFieldTypes), match,
      /*logging=*/{});
}

// static
void FormField::AddClassification(const AutofillField* field,
                                  ServerFieldType type,
                                  float score,
                                  FieldCandidatesMap& field_candidates) {
  // Several fields are optional.
  if (field == nullptr)
    return;

  FieldCandidates& candidates = field_candidates[field->global_id()];
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
  if (match_label &&
      MatchesRegexWithCache(label, pattern, capture_destination)) {
    found_match = true;
    match_type_string = "Match in label";
    value = label;
  } else if (match_type.attributes.contains(MatchAttribute::kName) &&
             MatchesRegexWithCache(name, pattern, capture_destination)) {
    found_match = true;
    match_type_string = "Match in name";
    value = name;
  } else if (match_label && pattern != kEmptyLabelRegex &&
             base::FeatureList::IsEnabled(
                 features::kAutofillAlwaysParsePlaceholders) &&
             MatchesRegexWithCache(field->placeholder, pattern,
                                   capture_destination)) {
    // Placeholders are matched against the same regexes as labels. However, to
    // prevent false positives in `ParseEmptyLabel()`, matches in placeholders
    // are explicitly prevented for `kEmptyLabelRegex`.
    // TODO(crbug.com/1317961): The label and placeholder cases should logically
    // be grouped together. Placeholder is currently last, because for the finch
    // study we want the group assignment to happen as late as possible.
    // Reorder once the change is rolled out.
    found_match = true;
    match_type_string = "Match in placeholder";
    value = field->placeholder;
  }

  if (found_match) {
    LogBuffer table_rows(IsLoggingActive(logging.log_manager));
    LOG_AF(table_rows) << Tr{} << "Match type:" << match_type_string;
    LOG_AF(table_rows) << Tr{} << "RegEx:" << logging.regex_name;
    LOG_AF(table_rows) << Tr{}
                       << "Value: " << HighlightValue(value, matches[0]);
    // The matched substring is reported once more as the highlighting is not
    // particularly copy&paste friendly.
    LOG_AF(table_rows) << Tr{} << "Matched substring: " << matches[0];
    LOG_AF(logging.log_manager)
        << LoggingScope::kParsing << LogMessage::kLocalHeuristicRegExMatched
        << Tag{"table"} << std::move(table_rows) << CTag{"table"};
  }

  return found_match;
}

// static
void FormField::ParseFormFieldsPass(ParseFunction parse,
                                    const std::vector<AutofillField*>& fields,
                                    FieldCandidatesMap& field_candidates,
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

  if (match_type.contains(MatchFieldType::kSelect) &&
      (type == "select-one" || type == "selectmenu")) {
    return true;
  }

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

// static
bool FormField::IsSingleFieldParseableType(ServerFieldType field_type) {
  return field_type == MERCHANT_PROMO_CODE || field_type == IBAN_VALUE ||
         field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
}

// static
void FormField::ParseUsingAutocompleteAttributes(
    const std::vector<AutofillField*>& fields,
    FieldCandidatesMap& field_candidates) {
  for (const AutofillField* field : fields) {
    HtmlFieldType html_type = FieldTypeFromAutocompleteAttributeValue(
        base::UTF16ToUTF8(field->parseable_name()));
    // The HTML_MODE is irrelevant when converting to a ServerFieldType.
    ServerFieldType type =
        AutofillType(html_type, HtmlFieldMode::kNone).GetStorableType();
    if (type != UNKNOWN_TYPE) {
      AddClassification(field, type, kBaseAutocompleteParserScore,
                        field_candidates);
    }
  }
}

}  // namespace autofill
