// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/is_required.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillRegexCache;
class AutofillScanner;
class FormFieldData;
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
  ParsingContext(base::span<const FormFieldData> fields,
                 GeoIpCountryCode client_country,
                 LanguageCode page_language,
                 PatternFile pattern_file,
                 DenseSet<RegexFeature> active_features,
                 LogManager* log_manager);
  ParsingContext(base::span<const std::unique_ptr<AutofillField>> fields,
                 GeoIpCountryCode client_country,
                 LanguageCode page_language,
                 PatternFile pattern_file,
                 DenseSet<RegexFeature> active_features,
                 LogManager* log_manager);
  ParsingContext(const ParsingContext&) = delete;
  ParsingContext& operator=(const ParsingContext&) = delete;
  ~ParsingContext();

  // Contains the parseable names that override FormFieldData::name().
  // Parsing code should prefer these names but fall back to
  // FormFieldData::name().
  base::flat_map<FieldGlobalId, std::u16string> name_overrides;

  // Contains the parseable labels that override FormFieldData::label().
  // Parsing code should prefer these labels but fall back to
  // FormFieldData::label().
  base::flat_map<FieldGlobalId, std::u16string> label_overrides;

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
  const bool better_placeholder_support{base::FeatureList::IsEnabled(
      features::kAutofillBetterLocalHeuristicPlaceholderSupport)};

  std::optional<RegexMatchesCache> matches_cache;
  raw_ref<AutofillRegexCache> regex_cache;

  raw_ptr<LogManager> log_manager;
};

// Represents a logical form field in a web form. Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormFieldParser {
 public:
  struct MatchInfo {
    // This is different from `autofill::MatchAttribute`, since it further
    // distinguishes between high and low quality labels. Low quality label
    // matches are deprioritized during scoring (`AddClassification()`), so a
    // different parser can overwrite the label match with e.g. a name match.
    // High quality labels are labels for which we have high confidence that the
    // label value is visible to the user and associated with the form control.
    // Low quality labels are heuristically determined labels which may be
    // incorrectly attributed to the form control.
    enum class MatchAttribute {
      kName = 0,
      kHighQualityLabel = 1,
      kLowQualityLabel = 2
    } matched_attribute = internal::IsRequired();
    // TODO(crbug.com/320965828): Add other details such as the regex that
    // matched or how well the regex matched to improve match prioritisation.
  };
  struct FieldAndMatchInfo {
    FieldAndMatchInfo(const FormFieldData* field LIFETIME_BOUND,
                      MatchInfo match_info)
        : field(*field), match_info(match_info) {}
    raw_ref<const FormFieldData> field = internal::IsRequired();
    MatchInfo match_info = internal::IsRequired();
  };

  FormFieldParser(const FormFieldParser&) = delete;
  FormFieldParser& operator=(const FormFieldParser&) = delete;

  virtual ~FormFieldParser() = default;

  // Classifies each field in |fields| with its heuristically detected type.
  // Each field has a derived unique name that is used as the key into
  // |field_candidates|.
  static void ParseFormFields(ParsingContext& context,
                              base::span<const FormFieldData> fields,
                              FieldCandidatesMap& field_candidates);

  // Looks for types that are allowed to appear in solitary (such as merchant
  // promo codes) inside |fields|. Each field has a derived unique name that is
  // used as the key into |field_candidates|.
  static void ParseSingleFields(ParsingContext& context,
                                base::span<const FormFieldData> fields,
                                FieldCandidatesMap& field_candidates);

  // Search for standalone loyalty card fields inside `fields`. Standalone
  // loyalty card fields are fields that should exclusively accept loyalty card
  // numbers, differentiating them from multi-purpose input fields that might
  // also accept emails or other data types
  static void ParseStandaloneLoyaltyCardFields(
      ParsingContext& context,
      base::span<const FormFieldData> fields,
      FieldCandidatesMap& field_candidates);

  // Search for standalone CVC fields inside `fields`. Standalone CVC fields
  // are CVC fields that should appear without any credit card field or email
  // address in the same form. Each field has a derived unique name that is
  // used as the key into `field_candidates`. Standalone CVC fields have unique
  // prerequisites in that there shouldn't be other credit card or email fields
  // in the form, which is why its parsing logic is extracted to its own method.
  static void ParseStandaloneCVCFields(ParsingContext& context,
                                       base::span<const FormFieldData> fields,
                                       FieldCandidatesMap& field_candidates);

  // Search for standalone email fields inside `fields`. Used because email
  // fields are commonly the only recognized field on account registration
  // sites. Currently called only when `kAutofillEnableEmailOnlyAddressForms` is
  // enabled.
  static void ParseStandaloneEmailFields(ParsingContext& context,
                                         base::span<const FormFieldData> fields,
                                         FieldCandidatesMap& field_candidates);

  // Returns a MatchInfo if `field` matches one of the the passed `patterns`.
  static std::optional<MatchInfo> FieldMatchesMatchPatternRef(
      ParsingContext& context,
      const FormFieldData& field,
      std::string_view regex_name,
      std::initializer_list<MatchParams (*)(const MatchParams&)> projections =
          {});

  // Removes entries from `field_candidates` in case
  // - not enough fields were classified by local heuristics.
  // - fields were not explicitly allow-listed because they appear in
  //   contexts that don't contain enough fields (e.g. forms with only an
  //   email address).
  static void ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      base::span<const FormFieldData> fields,
      FieldCandidatesMap& field_candidates,
      GeoIpCountryCode client_country,
      LogManager* log_manager);

 protected:
  friend class FormFieldParserTestApi;

  // Initial values assigned to FieldCandidates by their corresponding parsers.
  // There's an implicit precedence determined by the values assigned here.
  // Email is currently the most important followed by Phone, Travel, Address,
  // Credit Card, IBAN, Price, Loyalty Card, Name, Merchant promo code, and
  // Search.
  static constexpr float kBaseEmailParserScore = 1.4f;
  static constexpr float kBasePhoneParserScore = 1.3f;
  static constexpr float kBaseTravelParserScore = 1.2f;
  static constexpr float kBaseAddressParserScore = 1.1f;
  static constexpr float kBaseCreditCardParserScore = 1.0f;
  static constexpr float kBaseIbanParserScore = 0.975f;
  static constexpr float kBasePriceParserScore = 0.95f;
  static constexpr float kBaseLoyaltyCardParserScore = 0.95f;
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

  // Looks up the patterns using `regex_name` and attempts to parse a field
  // with them. Returns true on success and populates `match`.
  // If a `match_pattern_projection` is defined, it is applied to the pattern's
  // MatchParams after dereferencing the `MatchPatternRef`s.
  static bool ParseField(
      ParsingContext& context,
      const FormFieldData& field,
      std::string_view regex_name,
      std::optional<FieldAndMatchInfo>* match = nullptr,
      MatchParams (*match_pattern_projection)(const MatchParams&) = nullptr);

  // Applies the other overload of ParseField() to the next field of `scanner`
  // and advances `scanner` if successful.
  static bool ParseField(
      ParsingContext& context,
      AutofillScanner& scanner,
      std::string_view regex_name,
      std::optional<FieldAndMatchInfo>* match = nullptr,
      MatchParams (*match_pattern_projection)(const MatchParams&) = nullptr);

  // Attempts to parse a field with an empty label. Returns true
  // on success and fills |match| with a pointer to the field.
  static bool ParseEmptyLabel(ParsingContext& context,
                              AutofillScanner& scanner,
                              std::optional<FieldAndMatchInfo>* match);

  // Adds an association between a `match` and a `type` into `field_candidates`.
  // This association is weighted by `parser_score`, the higher the stronger the
  // association.
  // TODO(crbug.com/320965828): Don't just weight classifications based on a
  // `parser_score`, but also using `match.match_info`.
  static void AddClassification(const std::optional<FieldAndMatchInfo>& match,
                                FieldType type,
                                float parser_score,
                                FieldCandidatesMap& field_candidates);

  // Returns true iff `type` matches `match_type`.
  static bool MatchesFormControlType(FormControlType type,
                                     DenseSet<FormControlType> match_type);

 protected:
  // Derived classes must implement this interface to supply field type
  // information.  |ParseFormFields| coordinates the parsing and extraction
  // of types from an input vector of |FormFieldData| objects and delegates
  // the type extraction via this method.
  virtual void AddClassifications(
      FieldCandidatesMap& field_candidates) const = 0;

  // Attempts to parse several fields using the specified parsing functions in
  // arbitrary order. This is useful e.g. when parsing dates, where both dd/mm
  // and mm/dd makes sense.
  // Returns true if all fields were parsed successfully. In this case, the
  // fields are assigned with the matching ones.
  // If no order is matched every parser, false is returned, all fields are
  // reset to nullptr and the scanner is rewound to it's original position.
  static bool ParseInAnyOrder(
      AutofillScanner& scanner,
      base::span<const std::pair<raw_ptr<const FormFieldData>*,
                                 base::FunctionRef<bool()>>>
          fields_and_parsers);

 private:
  // Function pointer type for the parsing function that should be passed to the
  // ParseFormFieldsPass() helper function.
  typedef std::unique_ptr<FormFieldParser> ParseFunction(
      ParsingContext& context,
      AutofillScanner& scanner);

  // Matches the regular expression `pattern` against the specified
  // `match_attributes` of the `field`.
  static std::optional<MatchInfo> Match(
      ParsingContext& context,
      const FormFieldData& field,
      std::u16string_view pattern,
      DenseSet<MatchAttribute> match_attributes,
      std::string_view regex_name,
      bool is_negative_pattern = false);

  // Like `Match()`, but only for the label or name of the field.
  static std::optional<MatchInfo> MatchInLabel(
      ParsingContext& context,
      const FormFieldData& field,
      std::u16string_view pattern,
      std::string_view regex_name,
      bool is_negative_pattern = false);
  static std::optional<MatchInfo> MatchInName(ParsingContext& context,
                                              const FormFieldData& field,
                                              std::u16string_view pattern,
                                              std::string_view regex_name,
                                              bool is_negative_pattern = false);

  // Applies `parse()` from left to right to `fields`. Only considers fields
  // that satisfy `is_relevant()`.
  // Stores the classification results in `field_candidates`.
  static void ParseFormFieldsPass(ParseFunction parse,
                                  ParsingContext& context,
                                  base::span<const FormFieldData> fields,
                                  bool (*is_relevant)(const FormFieldData&),
                                  FieldCandidatesMap& field_candidates);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_PARSER_H_
