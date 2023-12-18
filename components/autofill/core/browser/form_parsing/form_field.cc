// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/form_field.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>

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

AutofillRegexCache& GetAutofillRegexCache() {
  // TODO(crbug.com/1309848): If ParseForm() is called from the same thread,
  // use a thread-unsafe parser.
  static base::NoDestructor<AutofillRegexCache> cache(ThreadSafe(true));
  return *cache;
}

}  // namespace

RegexMatchesCache::RegexMatchesCache(int capacity) : cache_(capacity) {}
RegexMatchesCache::~RegexMatchesCache() = default;

RegexMatchesCache::Key RegexMatchesCache::BuildKey(
    base::StringPiece16 input,
    base::StringPiece16 pattern) {
  return Key(std::hash<std::u16string_view>{}(input),
             std::hash<std::u16string_view>{}(pattern));
}

absl::optional<bool> RegexMatchesCache::Get(RegexMatchesCache::Key key) {
  if (auto it = cache_.Get(key); it != cache_.end()) {
    return it->second;
  }
  return absl::nullopt;
}

void RegexMatchesCache::Put(RegexMatchesCache::Key key, bool value) {
  cache_.Put(key, value);
}

ParsingContext::ParsingContext(GeoIpCountryCode client_country,
                               LanguageCode page_language,
                               PatternSource pattern_source)
    : client_country(std::move(client_country)),
      page_language(std::move(page_language)),
      pattern_source(pattern_source),
      regex_cache(GetAutofillRegexCache()) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableCacheForRegexMatching)) {
    matches_cache.emplace(
        features::kAutofillEnableCacheForRegexMatchingCacheSizeParam.Get());
  }
}

ParsingContext::~ParsingContext() = default;

// static
bool FormField::MatchesRegexWithCache(ParsingContext& context,
                                      base::StringPiece16 input,
                                      base::StringPiece16 pattern,
                                      std::vector<std::u16string>* groups) {
  RegexMatchesCache::Key key;
  if (!groups && context.matches_cache) {
    key = RegexMatchesCache::BuildKey(input, pattern);
    absl::optional<bool> cache_entry = context.matches_cache->Get(key);
    if (cache_entry.has_value()) {
      return cache_entry.value();
    }
  }
  const icu::RegexPattern* regex_pattern =
      context.regex_cache->GetRegexPattern(pattern);
  bool result = autofill::MatchesRegex(input, *regex_pattern, groups);
  if (!groups && context.matches_cache) {
    context.matches_cache->Put(key, result);
  }
  return result;
}

// static
void FormField::ParseFormFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    bool is_form_tag,
    FieldCandidatesMap& field_candidates,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, context, processed_fields,
                      field_candidates, log_manager);
  bool found_email_field = !field_candidates.empty();

  // Phone pass.
  ParseFormFieldsPass(PhoneField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // Travel pass.
  ParseFormFieldsPass(TravelField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // Address pass.
  ParseFormFieldsPass(AddressField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // Birthdate pass.
  if (base::FeatureList::IsEnabled(features::kAutofillEnableBirthdateParsing)) {
    ParseFormFieldsPass(BirthdateField::Parse, context, processed_fields,
                        field_candidates, log_manager);
  }

  // Numeric quantity pass.
  ParseFormFieldsPass(NumericQuantityField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  const size_t candidates_size = field_candidates.size();
  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, context, processed_fields,
                      field_candidates, log_manager);
  bool found_cc_fields = candidates_size != field_candidates.size();
  if (base::FeatureList::IsEnabled(
          features::kAutofillParseVcnCardOnFileStandaloneCvcFields) &&
      !found_email_field && !found_cc_fields) {
    // No email or cc fields found. Standalone CVC field pass for the VCN card
    // on file case.
    ParseStandaloneCVCFields(context, fields, field_candidates, log_manager);
    // Any detected standalone cvc fields are considered fillable single fields.
  }

  // Price pass.
  ParseFormFieldsPass(PriceField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // Search pass.
  ParseFormFieldsPass(SearchField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // Deduce `field_candidates` for the `processed_fields` by parsing their
  // `parsable_name()` as an autocomplete attribute.
  if (base::FeatureList::IsEnabled(
          features::kAutofillParseNameAsAutocompleteType)) {
    ParseUsingAutocompleteAttributes(processed_fields, field_candidates);
  }

  // Single fields pass.
  ParseSingleFieldForms(context, fields, is_form_tag, field_candidates,
                        log_manager);

  ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      context, fields, field_candidates, is_form_tag, log_manager);
}

// static
void FormField::ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates,
    bool is_form_tag,
    LogManager* log_manager) {
  // Set to count distinct field types.
  FieldTypeSet heuristic_types;
  for (const auto& [field_id, candidates] : field_candidates) {
    if (FieldType heuristic_type = candidates.BestHeuristicType();
        IsFillableFieldType(heuristic_type)) {
      heuristic_types.insert(heuristic_type);
    }
  }

  // We consider the number of distinct fillable field types, not the number of
  // distinct fillable fields, to determine whether local heuristics should be
  // applied. This reduces false positives by counting similar fields only once.
  // "Fillable" refers to the field type, not whether a specific field is
  // visible and editable by the user.
  size_t fillable_distinct_field_types = heuristic_types.size();

  // Do not autofill a form if there aren't enough fields. Otherwise, it is
  // very easy to have false positives. See http://crbug.com/447332
  // For <form> tags, make an exception for email fields, which are commonly
  // the only recognized field on account registration sites. Also make an
  // exception for single-field Autofillable types, even when the form contains
  // less than kMinRequiredFieldsForHeuristics fields in its form signature.
  if (fillable_distinct_field_types >= kMinRequiredFieldsForHeuristics) {
    return;
  }

  FieldTypeSet permitted_single_field_types{
      MERCHANT_PROMO_CODE, IBAN_VALUE,
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE};
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableZipOnlyAddressForms) &&
      AddressField::IsStandaloneZipSupported(context.client_country)) {
    permitted_single_field_types.insert(ADDRESS_HOME_ZIP);
  }

  // For historic reasons email addresses are only retained if they appear in
  // a <form> tag. It's unclear whether that's necessary.
  FieldTypeSet permitted_single_field_types_in_form{EMAIL_ADDRESS};

  // Returns whether a field type may exist as a stand-alone field.
  auto retainable_field_type =
      [&is_form_tag, &permitted_single_field_types_in_form,
       &permitted_single_field_types](FieldType heuristic_type) {
        return (is_form_tag && permitted_single_field_types_in_form.contains(
                                   heuristic_type)) ||
               permitted_single_field_types.contains(heuristic_type);
      };

  struct WipedField {
    FieldGlobalId field_id;
    FieldType best_heuristic_type;
  };
  std::vector<WipedField> wiped_fields;
  if (IsLoggingActive(log_manager)) {
    for (const auto& [field_id, candidates] : field_candidates) {
      FieldType heuristic_type = candidates.BestHeuristicType();
      if (!retainable_field_type(heuristic_type)) {
        wiped_fields.emplace_back(WipedField{field_id, heuristic_type});
      }
    }
  }

  // Given that we don't have kMinRequiredFieldsForHeuristics distinct field
  // types, we expect field_candidates to be small and don't need to be
  // extremely performant. It's ok to use EraseIf even if in some cases we could
  // clear everything.
  base::EraseIf(
      field_candidates,
      [&retainable_field_type](
          const FieldCandidatesMap::container_type::value_type& candidate) {
        return !retainable_field_type(candidate.second.BestHeuristicType());
      });

  if (IsLoggingActive(log_manager)) {
    LogBuffer table_rows;
    for (const auto& field : fields) {
      LOG_AF(table_rows) << Tr{} << "Field:" << *field;
    }
    for (const auto& f : wiped_fields) {
      LogBuffer name;
      name << "Type candidate for frame and renderer ID: " << f.field_id;

      LogBuffer description;
      LOG_AF(description) << "BestHeuristicType: "
                          << FieldTypeToStringView(f.best_heuristic_type)
                          << ", is fillable: "
                          << IsFillableFieldType(f.best_heuristic_type);

      LOG_AF(table_rows) << Tr{} << std::move(name) << std::move(description);
    }
    LOG_AF(log_manager)
        << LoggingScope::kParsing
        << LogMessage::kLocalHeuristicDidNotFindEnoughFillableFields
        << Tag{"table"} << Attrib{"class", "form"} << std::move(table_rows)
        << CTag{"table"};
  }
}

void FormField::ParseSingleFieldForms(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    bool is_form_tag,
    FieldCandidatesMap& field_candidates,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);

  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  // IBAN pass.
  ParseFormFieldsPass(IbanField::Parse, context, processed_fields,
                      field_candidates, log_manager);

  if (AddressField::IsStandaloneZipSupported(context.client_country)) {
    // In some countries we observe address forms that are particularly small
    // (e.g. only a zip code.)
    ParseFormFieldsPass(AddressField::ParseStandaloneZip, context,
                        processed_fields, field_candidates, log_manager);
  }
}

void FormField::ParseStandaloneCVCFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);
  ParseFormFieldsPass(StandaloneCvcField::Parse, context, processed_fields,
                      field_candidates, log_manager);
}

void FormField::ParseStandaloneEmailFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates,
    LogManager* log_manager) {
  std::vector<AutofillField*> processed_fields = RemoveCheckableFields(fields);
  ParseFormFieldsPass(EmailField::Parse, context, processed_fields,
                      field_candidates, log_manager);
}

// static
bool FormField::FieldMatchesMatchPatternRef(
    ParsingContext& context,
    base::span<const MatchPatternRef> patterns,
    const AutofillField& field,
    const RegExLogging& logging) {
  for (MatchPatternRef pattern_ref : patterns) {
    MatchingPattern pattern = *pattern_ref;
    if (!MatchesFormControlType(
            FormControlTypeToString(field.form_control_type),
            pattern.match_field_input_types)) {
      continue;
    }

    // For each of the two match field attributes, kName and kLabel, that are
    // active for the current pattern, test if the attribute matches the
    // negative pattern. If yes, remove it from the attributes that are
    // considered for positive matching.
    MatchParams match_type(pattern.match_field_attributes,
                           pattern.match_field_input_types);

    if (!IsEmpty(pattern.negative_pattern)) {
      for (MatchAttribute attribute : pattern.match_field_attributes) {
        if (FormField::Match(context, &field, pattern.negative_pattern,
                             MatchParams({attribute}, match_type.field_types),
                             logging)) {
          match_type.attributes.erase(attribute);
        }
      }
    }

    if (match_type.attributes.empty()) {
      continue;
    }

    if (!IsEmpty(pattern.positive_pattern) &&
        FormField::Match(context, &field, pattern.positive_pattern, match_type,
                         logging)) {
      return true;
    }
  }
  return false;
}

// static
bool FormField::ParseField(ParsingContext& context,
                           AutofillScanner* scanner,
                           base::StringPiece16 pattern,
                           base::span<const MatchPatternRef> patterns,
                           raw_ptr<AutofillField>* match,
                           const RegExLogging& logging) {
  return ParseFieldSpecifics(context, scanner, pattern, kDefaultMatchParams,
                             patterns, match, logging);
}

// static
bool FormField::ParseFieldSpecificsWithLegacyPattern(
    ParsingContext& context,
    AutofillScanner* scanner,
    base::StringPiece16 pattern,
    MatchParams match_type,
    raw_ptr<AutofillField>* match,
    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(FormControlTypeToString(field->form_control_type),
                              match_type.field_types)) {
    return false;
  }

  return MatchAndAdvance(context, scanner, pattern, match_type, match, logging);
}

// static
bool FormField::ParseFieldSpecificsWithNewPatterns(
    ParsingContext& context,
    AutofillScanner* scanner,
    base::span<const MatchPatternRef> patterns,
    raw_ptr<AutofillField>* match,
    const RegExLogging& logging,
    MatchingPattern (*projection)(const MatchingPattern&)) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  for (MatchPatternRef pattern_ref : patterns) {
    MatchingPattern pattern =
        projection ? (*projection)(*pattern_ref) : *pattern_ref;
    if (!MatchesFormControlType(
            FormControlTypeToString(field->form_control_type),
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
        if (FormField::Match(context, field, pattern.negative_pattern,
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
        MatchAndAdvance(context, scanner, pattern.positive_pattern, match_type,
                        match, logging)) {
      return true;
    }
  }
  return false;
}

// static
bool FormField::ParseFieldSpecifics(
    ParsingContext& context,
    AutofillScanner* scanner,
    base::StringPiece16 pattern,
    const MatchParams& match_type,
    base::span<const MatchPatternRef> patterns,
    raw_ptr<AutofillField>* match,
    const RegExLogging& logging,
    MatchingPattern (*projection)(const MatchingPattern&)) {
  return (base::FeatureList::IsEnabled(
              features::kAutofillParsingPatternProvider) ||
          // Some patterns may not exist as an old-school regex because they
          // require negative matching.
          pattern == kNoLegacyPattern)
             ? ParseFieldSpecificsWithNewPatterns(context, scanner, patterns,
                                                  match, logging, projection)
             : ParseFieldSpecificsWithLegacyPattern(context, scanner, pattern,
                                                    match_type, match, logging);
}

// static
bool FormField::ParseInAnyOrder(
    AutofillScanner* scanner,
    std::vector<
        std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
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
    if (matches) {
      return true;
    }
    scanner->RewindTo(original_pos);
  } while (std::next_permutation(p.begin(), p.end()));
  for (const auto& [field, _] : fields_and_parsers)
    *field = nullptr;
  return false;
}

// static
bool FormField::ParseEmptyLabel(ParsingContext& context,
                                AutofillScanner* scanner,
                                raw_ptr<AutofillField>* match) {
  return ParseFieldSpecificsWithLegacyPattern(
      context, scanner, kEmptyLabelRegex,
      MatchParams({MatchAttribute::kLabel}, kAllMatchFieldTypes), match,
      /*logging=*/{});
}

// static
void FormField::AddClassification(const AutofillField* field,
                                  FieldType type,
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
         !field->IsSelectElement())) {
      continue;
    }
    processed_fields.push_back(field.get());
  }
  return processed_fields;
}

// static
bool FormField::MatchAndAdvance(ParsingContext& context,
                                AutofillScanner* scanner,
                                base::StringPiece16 pattern,
                                MatchParams match_type,
                                raw_ptr<AutofillField>* match,
                                const RegExLogging& logging) {
  AutofillField* field = scanner->Cursor();
  if (FormField::Match(context, field, pattern, match_type, logging)) {
    if (match)
      *match = field;
    scanner->Advance();
    return true;
  }

  return false;
}

bool FormField::Match(ParsingContext& context,
                      const AutofillField* field,
                      base::StringPiece16 pattern,
                      MatchParams match_type,
                      const RegExLogging& logging) {
  bool found_match = false;
  std::string_view match_type_string;
  base::StringPiece16 value;
  std::vector<std::u16string> matches;
  std::vector<std::u16string>* capture_destination =
      logging.log_manager && logging.log_manager->IsLoggingActive() ? &matches
                                                                    : nullptr;

  // TODO(crbug/1165780): Remove once shared labels are launched.
  const std::u16string& label =
      context.autofill_enable_support_for_parsing_with_shared_labels
          ? field->parseable_label()
          : field->label;

  const std::u16string& name = field->parseable_name();

  const bool match_label =
      match_type.attributes.contains(MatchAttribute::kLabel);
  if (match_label &&
      MatchesRegexWithCache(context, label, pattern, capture_destination)) {
    found_match = true;
    match_type_string = "Match in label";
    value = label;
  } else if (match_type.attributes.contains(MatchAttribute::kName) &&
             MatchesRegexWithCache(context, name, pattern,
                                   capture_destination)) {
    found_match = true;
    match_type_string = "Match in name";
    value = name;
  } else if (match_label && pattern != kEmptyLabelRegex &&
             context.autofill_always_parse_placeholders &&
             MatchesRegexWithCache(context, field->placeholder, pattern,
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

  if (found_match && capture_destination) {
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
                                    ParsingContext& context,
                                    const std::vector<AutofillField*>& fields,
                                    FieldCandidatesMap& field_candidates,
                                    LogManager* log_manager) {
  AutofillScanner scanner(fields);
  while (!scanner.IsEnd()) {
    std::unique_ptr<FormField> form_field =
        parse(context, &scanner, log_manager);
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
bool FormField::MatchesFormControlType(std::string_view type,
                                       DenseSet<MatchFieldType> match_type) {
  if (match_type.contains(MatchFieldType::kText) && type == "text")
    return true;

  if (match_type.contains(MatchFieldType::kEmail) && type == "email")
    return true;

  if (match_type.contains(MatchFieldType::kTelephone) && type == "tel")
    return true;

  if (match_type.contains(MatchFieldType::kSelect) &&
      (type == "select-one" || type == "selectlist")) {
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
bool FormField::IsSingleFieldParseableType(FieldType field_type) {
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
    // The HTML_MODE is irrelevant when converting to a FieldType.
    FieldType type = AutofillType(html_type).GetStorableType();
    if (type != UNKNOWN_TYPE) {
      AddClassification(field, type, kBaseAutocompleteParserScore,
                        field_candidates);
    }
  }
}

}  // namespace autofill
