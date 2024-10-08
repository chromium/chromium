// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillRegexCache;
class AutofillScanner;
class LogManager;

// LRU cache to prevent the repetitive evaluation of identical regular
// expressions (`pattern`) on identical `input` strings.
class RegexMatchesCache {
 public:
  // A good capacity for the cache according to an empirical study of forms
  // with AllForms/HeuristicClassificationTests.EndToEnd is 300. This needs
  // to be confirmed in real world experiments.
  // RegexMatchesCache is not intended as a permanent cache but instantiated
  // once per form parsing, so this is not allocating a lot of memory
  // permanently.
  explicit RegexMatchesCache(int capacity);
  RegexMatchesCache(const RegexMatchesCache&) = delete;
  RegexMatchesCache& operator=(const RegexMatchesCache&) = delete;
  ~RegexMatchesCache();

  // Hash values of an input and a pattern. There is a theoretical risk of
  // collision which we are accepting here to not store the inputs an patterns
  // which both may be large. Given that our heuristics are not 100% accurate
  // the small risk of a collision seems acceptable.
  // TODO(crbug.com/40146444): Once we don't use autofill_regex_constants.h
  // anymore, the second `std::size_t` should probably be a MatchPatternRef:
  // - more accurate (they uniquely identify the pattern across all pattern
  //   sources),
  // - more time-efficient (no hashing needed),
  // - more space-efficient (2 vs 8 bytes)
  using Key = std::pair<std::size_t, std::size_t>;

  // Creates a key for an `input` string and a `pattern` to be used in the LRU
  // cache.
  static Key BuildKey(std::u16string_view input, std::u16string_view pattern);

  // Returns whether `pattern` in the key matched `input` if this information is
  // cached. std::nullopt if the information is not cached.
  std::optional<bool> Get(Key key);

  // Stores whether `pattern` in the key matched `input`.
  void Put(Key key, bool value);

 private:
  struct Hasher {
    std::size_t operator()(const Key& key) const noexcept {
      return std::get<0>(key) ^ std::get<1>(key);
    }
  };

  base::HashingLRUCache<Key, bool, Hasher> cache_;
};

// This is a helper class that is instantiated before form parsing. It contains
// a) environmental information that is needed in many places and b) caches to
// prevent repetitive work.
struct ParsingContext {
  ParsingContext(GeoIpCountryCode client_country,
                 LanguageCode page_language,
                 PatternFile pattern_file,
                 DenseSet<RegexFeature> active_features = {},
                 LogManager* log_manager = nullptr);
  ParsingContext(const ParsingContext&) = delete;
  ParsingContext& operator=(const ParsingContext&) = delete;
  ~ParsingContext();

  const GeoIpCountryCode client_country;
  const LanguageCode page_language;
  // Mutable so that the caches can be reused across different pattern files
  // and active features. Since the cache works at a regex level, this is safe.
  PatternFile pattern_file;
  DenseSet<RegexFeature> active_features;

  // Cache for autofill features that are tested on hot code paths. Testing
  // whether a feature is enabled is pretty expensive. Caching the status of two
  // features led to a performance improvement for form field classification of
  // 19% in release builds.
  // Note that adding features here may push users into the respective
  // experiment/control groups earlier than you may want.
  const bool autofill_enable_support_for_parsing_with_shared_labels{
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForParsingWithSharedLabels)};
  const bool autofill_always_parse_placeholders{
      base::FeatureList::IsEnabled(features::kAutofillAlwaysParsePlaceholders)};

  std::optional<RegexMatchesCache> matches_cache;
  raw_ref<AutofillRegexCache> regex_cache;

  raw_ptr<LogManager> log_manager;
};

// Represents a logical form field in a web form. Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormFieldParser {
 public:
  FormFieldParser(const FormFieldParser&) = delete;
  FormFieldParser& operator=(const FormFieldParser&) = delete;

  virtual ~FormFieldParser() = default;

  // Classifies each field in |fields| with its heuristically detected type.
  // Each field has a derived unique name that is used as the key into
  // |field_candidates|.
  static void ParseFormFields(
      ParsingContext& context,
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      bool is_form_tag,
      FieldCandidatesMap& field_candidates);

  // Looks for types that are allowed to appear in solitary (such as merchant
  // promo codes) inside |fields|. Each field has a derived unique name that is
  // used as the key into |field_candidates|.
  static void ParseSingleFieldForms(
      ParsingContext& context,
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      FieldCandidatesMap& field_candidates);

  // Search for standalone CVC fields inside `fields`. Standalone CVC fields
  // are CVC fields that should appear without any credit card field or email
  // address in the same form. Each field has a derived unique name that is
  // used as the key into `field_candidates`. Standalone CVC fields have unique
  // prerequisites in that there shouldn't be other credit card or email fields
  // in the form, which is why its parsing logic is extracted to its own method.
  static void ParseStandaloneCVCFields(
      ParsingContext& context,
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      FieldCandidatesMap& field_candidates);

  // Search for standalone email fields inside `fields`. Used because email
  // fields are commonly the only recognized field on account registration
  // sites. Currently called only when `kAutofillEnableEmailOnlyAddressForms` is
  // enabled.
  static void ParseStandaloneEmailFields(
      ParsingContext& context,
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      FieldCandidatesMap& field_candidates);

  // Returns true if `field` matches one of the the passed `patterns`.
  static bool FieldMatchesMatchPatternRef(
      ParsingContext& context,
      base::span<const MatchPatternRef> patterns,
      const AutofillField& field,
      const char* regex_name = "",
      std::initializer_list<MatchParams (*)(const MatchParams&)> projections =
          {});

#if defined(UNIT_TEST)
  static bool MatchForTesting(ParsingContext& context,
                              const AutofillField* field,
                              std::u16string_view pattern,
                              DenseSet<MatchAttribute> match_attributes,
                              const char* regex_name = "") {
    return FormFieldParser::Match(context, field, pattern, match_attributes,
                                  regex_name);
  }

  static bool ParseInAnyOrderForTesting(
      AutofillScanner* scanner,
      std::vector<
          std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
          fields_and_parsers) {
    return FormFieldParser::ParseInAnyOrder(scanner, fields_and_parsers);
  }

  // Assign types to the fields for the testing purposes.
  void AddClassificationsForTesting(
      FieldCandidatesMap& field_candidates_for_testing) const {
    AddClassifications(field_candidates_for_testing);
  }
#endif

 protected:
  // Initial values assigned to FieldCandidates by their corresponding parsers.
  // There's an implicit precedence determined by the values assigned here.
  // Email is currently the most important followed by Phone, Travel, Address,
  // Credit Card, IBAN, Price, Name, Merchant promo code, and Search.
  static constexpr float kBaseEmailParserScore = 1.4f;
  static constexpr float kBasePhoneParserScore = 1.3f;
  static constexpr float kBaseTravelParserScore = 1.2f;
  static constexpr float kBaseAddressParserScore = 1.1f;
  static constexpr float kBaseCreditCardParserScore = 1.0f;
  static constexpr float kBaseIbanParserScore = 0.975f;
  static constexpr float kBasePriceParserScore = 0.95f;
  static constexpr float kBaseNameParserScore = 0.9f;
  static constexpr float kBaseMerchantPromoCodeParserScore = 0.85f;
  static constexpr float kBaseSearchParserScore = 0.8f;
  static constexpr float kBaseImprovedPredictionsScore = 0.7f;

  // Only derived classes may instantiate.
  FormFieldParser() = default;

  // Calls MatchesRegex() with a thread-safe cache.
  // Should not be called from the UI thread as it may be blocked on a worker
  // thread.
  static bool MatchesRegexWithCache(
      ParsingContext& context,
      std::u16string_view input,
      std::u16string_view pattern,
      std::vector<std::u16string>* groups = nullptr);

  // Attempts to parse a form field with the given pattern.  Returns true on
  // success and fills `match` with a pointer to the field.
  // If a `match_pattern_projection` is defined, it is applied to the pattern's
  // MatchParams after dereferencing the `MatchPatternRef`s.
  static bool ParseField(
      ParsingContext& context,
      AutofillScanner* scanner,
      base::span<const MatchPatternRef> patterns,
      raw_ptr<AutofillField>* match,
      const char* regex_name = "",
      MatchParams (*match_pattern_projection)(const MatchParams&) = nullptr);

  // Attempts to parse a field with an empty label. Returns true
  // on success and fills |match| with a pointer to the field.
  static bool ParseEmptyLabel(ParsingContext& context,
                              AutofillScanner* scanner,
                              raw_ptr<AutofillField>* match);

  // Attempts to parse several fields using the specified parsing functions in
  // arbitrary order. This is useful e.g. when parsing dates, where both dd/mm
  // and mm/dd makes sense.
  // Returns true if all fields were parsed successfully. In this case, the
  // fields are assigned with the matching ones.
  // If no order is matched every parser, false is returned, all fields are
  // reset to nullptr and the scanner is rewound to it's original position.
  static bool ParseInAnyOrder(
      AutofillScanner* scanner,
      std::vector<
          std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
          fields_and_parsers);

  // Adds an association between a |field| and a |type| into |field_candidates|.
  // This association is weighted by |score|, the higher the stronger the
  // association.
  static void AddClassification(const AutofillField* field,
                                FieldType type,
                                float score,
                                FieldCandidatesMap& field_candidates);

  // Returns true iff `type` matches `match_type`.
  static bool MatchesFormControlType(FormControlType type,
                                     DenseSet<FormControlType> match_type);

 protected:
  // Returns true if |field_type| is a single field parseable type.
  static bool IsSingleFieldParseableType(FieldType field_type);

  // Derived classes must implement this interface to supply field type
  // information.  |ParseFormFields| coordinates the parsing and extraction
  // of types from an input vector of |AutofillField| objects and delegates
  // the type extraction via this method.
  virtual void AddClassifications(
      FieldCandidatesMap& field_candidates) const = 0;

 private:
  // Function pointer type for the parsing function that should be passed to the
  // ParseFormFieldsPass() helper function.
  typedef std::unique_ptr<FormFieldParser> ParseFunction(
      ParsingContext& context,
      AutofillScanner* scanner);

  // Removes entries from `field_candidates` in case
  // - not enough fields were classified by local heuristics.
  // - fields were not explicitly allow-listed because they appear in
  //   contexts that don't contain enough fields (e.g. forms with only an
  //   email address).
  static void ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      ParsingContext& context,
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      FieldCandidatesMap& field_candidates,
      bool is_form_tag);

  // Removes checkable fields and returns fields to be processed for field
  // detection.
  static std::vector<raw_ptr<AutofillField, VectorExperimental>>
  RemoveCheckableFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields);

  // Matches the regular expression |pattern| against the components of
  // |field| as specified in |match_type|.
  static bool Match(ParsingContext& context,
                    const AutofillField* field,
                    std::u16string_view pattern,
                    DenseSet<MatchAttribute> match_attributes,
                    const char* regex_name = "");

  // Perform a "pass" over the |fields| where each pass uses the supplied
  // |parse| method to match content to a given field type.
  // |fields| is both an input and an output parameter.  Upon exit |fields|
  // holds any remaining unclassified fields for further processing.
  // Classification results of the processed fields are stored in
  // |field_candidates|.
  static void ParseFormFieldsPass(
      ParseFunction parse,
      ParsingContext& context,
      const std::vector<raw_ptr<AutofillField, VectorExperimental>>& fields,
      FieldCandidatesMap& field_candidates);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_H_
