// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <string_view>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser_ng.h"
#include "components/autofill/core/browser/form_parsing/alternative_name_field_parser.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field_parser.h"
#include "components/autofill/core/browser/form_parsing/email_field_parser.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_parsing/iban_field_parser.h"
#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field_parser.h"
#include "components/autofill/core/browser/form_parsing/name_field_parser.h"
#include "components/autofill/core/browser/form_parsing/phone_field_parser.h"
#include "components/autofill/core/browser/form_parsing/prediction_improvements_field_parser.h"
#include "components/autofill/core/browser/form_parsing/price_field_parser.h"
#include "components/autofill/core/browser/form_parsing/search_field_parser.h"
#include "components/autofill/core/browser/form_parsing/standalone_cvc_field_parser.h"
#include "components/autofill/core/browser/form_parsing/travel_field_parser.h"
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

AutofillRegexCache& GetAutofillRegexCache() {
  // TODO(crbug.com/40219607): If ParseForm() is called from the same thread,
  // use a thread-unsafe parser.
  static base::NoDestructor<AutofillRegexCache> cache(ThreadSafe(true));
  return *cache;
}

}  // namespace

RegexMatchesCache::RegexMatchesCache(int capacity) : cache_(capacity) {}
RegexMatchesCache::~RegexMatchesCache() = default;

RegexMatchesCache::Key RegexMatchesCache::BuildKey(
    std::u16string_view input,
    std::u16string_view pattern) {
  return Key(std::hash<std::u16string_view>{}(input),
             std::hash<std::u16string_view>{}(pattern));
}

std::optional<bool> RegexMatchesCache::Get(RegexMatchesCache::Key key) {
  if (auto it = cache_.Get(key); it != cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void RegexMatchesCache::Put(RegexMatchesCache::Key key, bool value) {
  cache_.Put(key, value);
}

ParsingContext::ParsingContext(GeoIpCountryCode client_country,
                               LanguageCode page_language,
                               PatternFile pattern_file,
                               DenseSet<RegexFeature> active_features,
                               LogManager* log_manager)
    : client_country(std::move(client_country)),
      page_language(std::move(page_language)),
      pattern_file(pattern_file),
      active_features(active_features),
      regex_cache(GetAutofillRegexCache()),
      log_manager(log_manager) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableCacheForRegexMatching)) {
    matches_cache.emplace(
        features::kAutofillEnableCacheForRegexMatchingCacheSizeParam.Get());
  }
}

ParsingContext::~ParsingContext() = default;

// static
bool FormFieldParser::MatchesRegexWithCache(
    ParsingContext& context,
    std::u16string_view input,
    std::u16string_view pattern,
    std::vector<std::u16string>* groups) {
  RegexMatchesCache::Key key;
  if (!groups && context.matches_cache) {
    key = RegexMatchesCache::BuildKey(input, pattern);
    std::optional<bool> cache_entry = context.matches_cache->Get(key);
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
void FormFieldParser::ParseFormFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    bool is_form_tag,
    FieldCandidatesMap& field_candidates) {
  std::vector<raw_ptr<AutofillField, VectorExperimental>> processed_fields =
      RemoveCheckableFields(fields);

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  // Prediction improvements are parsed using their own exclusive pattern file.
  if (context.pattern_file == PatternFile::kPredictionImprovements) {
    ParseFormFieldsPass(PredictionImprovementsFieldParser::Parse, context,
                        processed_fields, field_candidates);
    return;
  }
#endif

  // Email pass.
  ParseFormFieldsPass(EmailFieldParser::Parse, context, processed_fields,
                      field_candidates);
  bool found_email_field = !field_candidates.empty();

  // Phone pass.
  ParseFormFieldsPass(PhoneFieldParser::Parse, context, processed_fields,
                      field_candidates);

  // Travel pass.
  ParseFormFieldsPass(TravelFieldParser::Parse, context, processed_fields,
                      field_candidates);

  // Address pass.
  ParseFormFieldsPass(base::FeatureList::IsEnabled(
                          features::kAutofillEnableAddressFieldParserNG)
                          ? AddressFieldParserNG::Parse
                          : AddressFieldParser::Parse,
                      context, processed_fields, field_candidates);

  const size_t candidates_size = field_candidates.size();
  // Credit card pass.
  ParseFormFieldsPass(CreditCardFieldParser::Parse, context, processed_fields,
                      field_candidates);
  bool found_cc_fields = candidates_size != field_candidates.size();
  if (base::FeatureList::IsEnabled(
          features::kAutofillParseVcnCardOnFileStandaloneCvcFields) &&
      !found_email_field && !found_cc_fields) {
    // No email or cc fields found. Standalone CVC field pass for the VCN card
    // on file case.
    ParseStandaloneCVCFields(context, fields, field_candidates);
    // Any detected standalone cvc fields are considered fillable single fields.
  }

  // Price pass.
  ParseFormFieldsPass(PriceFieldParser::Parse, context, processed_fields,
                      field_candidates);

  // Name pass.
  ParseFormFieldsPass(NameFieldParser::Parse, context, processed_fields,
                      field_candidates);

  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    // Alternative name (e.g. phonetic name) pass.
    ParseFormFieldsPass(AlternativeNameFieldParser::Parse, context,
                        processed_fields, field_candidates);
  }

  // Search pass.
  ParseFormFieldsPass(SearchFieldParser::Parse, context, processed_fields,
                      field_candidates);

  // Single fields pass.
  ParseSingleFieldForms(context, fields, field_candidates);

  ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      context, fields, field_candidates, is_form_tag);
}

// static
void FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates,
    bool is_form_tag) {
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
  if (AddressFieldParser::IsStandaloneZipSupported(context.client_country)) {
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
  if (IsLoggingActive(context.log_manager)) {
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

  if (IsLoggingActive(context.log_manager)) {
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
    LOG_AF(context.log_manager)
        << LoggingScope::kParsing
        << LogMessage::kLocalHeuristicDidNotFindEnoughFillableFields
        << Tag{"table"} << Attrib{"class", "form"} << std::move(table_rows)
        << CTag{"table"};
  }
}

void FormFieldParser::ParseSingleFieldForms(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates) {
  std::vector<raw_ptr<AutofillField, VectorExperimental>> processed_fields =
      RemoveCheckableFields(fields);
  // Merchant promo code pass.
  ParseFormFieldsPass(MerchantPromoCodeFieldParser::Parse, context,
                      processed_fields, field_candidates);

  // IBAN pass.
  ParseFormFieldsPass(IbanFieldParser::Parse, context, processed_fields,
                      field_candidates);

  if (AddressFieldParser::IsStandaloneZipSupported(context.client_country)) {
    // In some countries we observe address forms that are particularly small
    // (e.g. only a zip code.)
    ParseFormFieldsPass(AddressFieldParser::ParseStandaloneZip, context,
                        processed_fields, field_candidates);
  }
}

void FormFieldParser::ParseStandaloneCVCFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates) {
  std::vector<raw_ptr<AutofillField, VectorExperimental>> processed_fields =
      RemoveCheckableFields(fields);
  ParseFormFieldsPass(StandaloneCvcFieldParser::Parse, context,
                      processed_fields, field_candidates);
}

void FormFieldParser::ParseStandaloneEmailFields(
    ParsingContext& context,
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    FieldCandidatesMap& field_candidates) {
  std::vector<raw_ptr<AutofillField, VectorExperimental>> processed_fields =
      RemoveCheckableFields(fields);
  // Do not ignore fields with autocomplete attributes attempting to disable
  // autocomplete. Disabling autocomplete is a common practice on fields where
  // we don't want to offer email filling even if our heuristics match (e.g.
  // search input fields).

  if (features::kAutofillEnableEmailHeuristicAutocompleteEmail.Get()) {
    std::erase_if(processed_fields, [](const AutofillField* field) {
      return field->autocomplete_attribute() != "email";
    });
  } else {
    std::erase_if(processed_fields, [](const AutofillField* field) {
      return field->autocomplete_attribute() == "off" ||
             field->autocomplete_attribute() == "false";
    });
  }

  ParseFormFieldsPass(EmailFieldParser::Parse, context, processed_fields,
                      field_candidates);
}

// static
bool FormFieldParser::FieldMatchesMatchPatternRef(
    ParsingContext& context,
    base::span<const MatchPatternRef> patterns,
    const AutofillField& field,
    const char* regex_name,
    std::initializer_list<MatchParams (*)(const MatchParams&)> projections) {
  // Calling the regex engine with multiple smaller regexes is less efficient
  // than calling it with one larger regex. For this reasons, positive_patterns
  // are batched by OR-ing them together. Since matching further depends on the
  // MatchAttributes of the pattern, `batched_patterns` is used to group them by
  // MatchAttributes.
  base::flat_map<DenseSet<MatchAttribute>, std::vector<std::u16string_view>>
      batched_patterns;

  for (MatchPatternRef pattern_ref : patterns) {
    MatchingPattern pattern = *pattern_ref;
    CHECK(!IsEmpty(pattern.positive_pattern));
    MatchParams match_params(pattern.match_field_attributes,
                             pattern.form_control_types);
    for (auto projection : projections) {
      if (projection) {
        match_params = (*projection)(match_params);
      }
    }
    if (!MatchesFormControlType(field.form_control_type(),
                                match_params.field_types)) {
      continue;
    }
    if (!pattern.IsActive(context.active_features)) {
      continue;
    }

    DenseSet<MatchAttribute> reduced_attributes = match_params.attributes;
    if (!IsEmpty(pattern.negative_pattern)) {
      // For each attribute that is active for the current pattern, test if it
      // matches the negative pattern. If so, remove it from the attributes that
      // are considered for positive matching.
      for (MatchAttribute attribute : match_params.attributes) {
        if (Match(context, &field, pattern.negative_pattern, {attribute},
                  regex_name)) {
          reduced_attributes.erase(attribute);
        }
      }
      if (reduced_attributes.empty()) {
        continue;
      }
    }

    auto it = batched_patterns.lower_bound(reduced_attributes);
    if (it != batched_patterns.end() && it->first == reduced_attributes) {
      it->second.push_back((*pattern_ref).positive_pattern);
    } else {
      batched_patterns.insert(
          it, {reduced_attributes, {(*pattern_ref).positive_pattern}});
    }
  }
  for (const auto& [attributes, positive_patterns] : batched_patterns) {
    if (Match(context, &field, base::JoinString(positive_patterns, u"|"),
              attributes, regex_name)) {
      return true;
    }
  }
  return false;
}

// static
bool FormFieldParser::ParseField(
    ParsingContext& context,
    AutofillScanner* scanner,
    base::span<const MatchPatternRef> patterns,
    raw_ptr<AutofillField>* match,
    const char* regex_name,
    MatchParams (*projection)(const MatchParams&)) {
  if (scanner->IsEnd()) {
    return false;
  }
  AutofillField* field = scanner->Cursor();
  if (FieldMatchesMatchPatternRef(context, patterns, *field, regex_name,
                                  {projection})) {
    if (match) {
      *match = field;
    }
    scanner->Advance();
    return true;
  }
  return false;
}

// static
bool FormFieldParser::ParseInAnyOrder(
    AutofillScanner* scanner,
    std::vector<
        std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
        fields_and_parsers) {
  if (scanner->IsEnd()) {
    return fields_and_parsers.empty();
  }
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
  for (const auto& [field, _] : fields_and_parsers) {
    *field = nullptr;
  }
  return false;
}

// static
bool FormFieldParser::ParseEmptyLabel(ParsingContext& context,
                                      AutofillScanner* scanner,
                                      raw_ptr<AutofillField>* match) {
  if (scanner->IsEnd()) {
    return false;
  }
  // Temporarily disable logging of matches for empty labels. They don't contain
  // a lot of insights but occur somewhat often.
  base::AutoReset disable_logging(&context.log_manager, nullptr);
  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(
          field->form_control_type(),
          {FormControlType::kInputEmail, FormControlType::kInputNumber,
           FormControlType::kInputPassword, FormControlType::kInputSearch,
           FormControlType::kInputTelephone, FormControlType::kInputText,
           FormControlType::kSelectOne, FormControlType::kTextArea})) {
    return false;
  }
  if (Match(context, field, kEmptyLabelRegex, {MatchAttribute::kLabel},
            "kEmptyLabelRegex")) {
    if (match) {
      *match = field;
    }
    scanner->Advance();
    return true;
  }
  return false;
}

// static
void FormFieldParser::AddClassification(const AutofillField* field,
                                        FieldType type,
                                        float score,
                                        FieldCandidatesMap& field_candidates) {
  // Several fields are optional.
  if (field == nullptr) {
    return;
  }

  FieldCandidates& candidates = field_candidates[field->global_id()];
  candidates.AddFieldCandidate(type, score);
}

// static
std::vector<raw_ptr<AutofillField, VectorExperimental>>
FormFieldParser::RemoveCheckableFields(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  // Set up a working copy of the fields to be processed.
  std::vector<raw_ptr<AutofillField, VectorExperimental>> processed_fields;
  for (const auto& field : fields) {
    // Ignore checkable fields as they interfere with parsers assuming context.
    // Eg., while parsing address, "Is PO box" checkbox after ADDRESS_LINE1
    // interferes with correctly understanding ADDRESS_LINE2.
    // Ignore fields marked as presentational, unless for 'select' fields (for
    // synthetic fields.)
    if (IsCheckable(field->check_status()) ||
        (field->role() == FormFieldData::RoleAttribute::kPresentation &&
         !field->IsSelectElement())) {
      continue;
    }
    processed_fields.push_back(field.get());
  }
  return processed_fields;
}

bool FormFieldParser::Match(ParsingContext& context,
                            const AutofillField* field,
                            std::u16string_view pattern,
                            DenseSet<MatchAttribute> match_attributes,
                            const char* regex_name) {
  bool found_match = false;
  std::string_view match_type_string;
  std::u16string_view value;
  std::vector<std::u16string> matches;
  std::vector<std::u16string>* capture_destination =
      context.log_manager && context.log_manager->IsLoggingActive() ? &matches
                                                                    : nullptr;

  // TODO(crbug.com/40741721): Remove once shared labels are launched.
  const std::u16string& label =
      context.autofill_enable_support_for_parsing_with_shared_labels
          ? field->parseable_label()
          : field->label();

  const std::u16string& name = field->parseable_name();

  const bool match_label = match_attributes.contains(MatchAttribute::kLabel);
  if (match_label &&
      MatchesRegexWithCache(context, label, pattern, capture_destination)) {
    found_match = true;
    match_type_string = "Match in label";
    value = label;
  } else if (match_attributes.contains(MatchAttribute::kName) &&
             MatchesRegexWithCache(context, name, pattern,
                                   capture_destination)) {
    found_match = true;
    match_type_string = "Match in name";
    value = name;
  } else if (match_label && pattern != kEmptyLabelRegex &&
             context.autofill_always_parse_placeholders &&
             MatchesRegexWithCache(context, field->placeholder(), pattern,
                                   capture_destination)) {
    // Placeholders are matched against the same regexes as labels. However, to
    // prevent false positives in `ParseEmptyLabel()`, matches in placeholders
    // are explicitly prevented for `kEmptyLabelRegex`.
    // TODO(crbug.com/40222716): The label and placeholder cases should
    // logically be grouped together. Placeholder is currently last, because for
    // the finch study we want the group assignment to happen as late as
    // possible. Reorder once the change is rolled out.
    found_match = true;
    match_type_string = "Match in placeholder";
    value = field->placeholder();
  }

  if (found_match && capture_destination) {
    LogBuffer table_rows(IsLoggingActive(context.log_manager));
    LOG_AF(table_rows) << Tr{} << "Match type:" << match_type_string;
    LOG_AF(table_rows) << Tr{} << "RegEx:" << regex_name;
    LOG_AF(table_rows) << Tr{}
                       << "Value: " << HighlightValue(value, matches[0]);
    // The matched substring is reported once more as the highlighting is not
    // particularly copy&paste friendly.
    LOG_AF(table_rows) << Tr{} << "Matched substring: " << matches[0];
    LOG_AF(context.log_manager)
        << LoggingScope::kParsing << LogMessage::kLocalHeuristicRegExMatched
        << Tag{"table"} << std::move(table_rows) << CTag{"table"};
  }

  return found_match;
}

// static
void FormFieldParser::ParseFormFieldsPass(
    ParseFunction parse,
    ParsingContext& context,
    const std::vector<raw_ptr<AutofillField, VectorExperimental>>& fields,
    FieldCandidatesMap& field_candidates) {
  AutofillScanner scanner(fields);
  while (!scanner.IsEnd()) {
    std::unique_ptr<FormFieldParser> form_field = parse(context, &scanner);
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
bool FormFieldParser::MatchesFormControlType(
    FormControlType type,
    DenseSet<FormControlType> match_type) {
  return match_type.contains(type);
}

// static
bool FormFieldParser::IsSingleFieldParseableType(FieldType field_type) {
  return field_type == MERCHANT_PROMO_CODE || field_type == IBAN_VALUE ||
         field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
}

}  // namespace autofill
