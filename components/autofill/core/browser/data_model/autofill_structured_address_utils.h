// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_UTILS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_rewriter.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

struct AddressToken {
  // The original value.
  std::u16string value;
  // The normalized value.
  std::u16string normalized_value;
  // The token position in the original string.
  int position;
};

enum class RegEx;

// Enum to express the few quantifiers needed to parse values.
enum class MatchQuantifier {
  // The capture group is required.
  kRequired,
  // The capture group is optional.
  kOptional,
  // The capture group is lazy optional meaning that it is avoided if an overall
  // match is possible.
  kLazyOptional,
};

// The result status of comparing two sets of sorted tokens.
enum class SortedTokenComparisonStatus {
  // The tokens are neither the same nor super/sub sets.
  kDistinct,
  // The token exactly match.
  kMatch,
  // The first value is a subset of the second.
  kSubset,
  // The first value is a superset of the other.
  kSuperset
};

// The result from comparing two sets of sorted tokens containing the status and
// the additional tokens in the super/sub sets.
struct SortedTokenComparisonResult {
  explicit SortedTokenComparisonResult(
      SortedTokenComparisonStatus status,
      std::vector<AddressToken> additional_tokens = {});
  SortedTokenComparisonResult(SortedTokenComparisonResult&& other);
  SortedTokenComparisonResult& operator=(SortedTokenComparisonResult&& other);
  ~SortedTokenComparisonResult();
  // The status of the token comparison.
  SortedTokenComparisonStatus status = SortedTokenComparisonStatus::kDistinct;
  // The additional elements in the super/subsets.
  std::vector<AddressToken> additional_tokens{};
  // Returns true if the first is a subset of the second;
  bool IsSingleTokenSubset() const;
  // Return true if the first is a superset of the second;
  bool IsSingleTokenSuperset() const;
  // Return true if one is a subset of the other.
  bool OneIsSubset() const;
  // Returns true if one contains the other.
  bool ContainEachOther() const;
  // Returns true if both contain the same tokens.
  bool TokensMatch() const;
};

// Options for capturing a named group using the
// |CaptureTypeWithPattern(...)| functions.
struct CaptureOptions {
  // A separator that must be matched after a capture group.
  // By default, a group must be either followed by a space-like character (\s)
  // or it must be the last group in the line. The separator is allowed to be
  // empty.
  std::string separator = ",|\\s+|$";
  // Indicates if the group is required, optional or even lazy optional.
  MatchQuantifier quantifier = MatchQuantifier::kRequired;
};

// A cache for compiled RE2 regular expressions.
class Re2RegExCache {
 public:
  Re2RegExCache& operator=(const Re2RegExCache&) = delete;
  Re2RegExCache(const Re2RegExCache&) = delete;
  ~Re2RegExCache() = delete;

  // Returns a singleton instance.
  static Re2RegExCache* Instance();

  // Returns a pointer to a constant compiled expression that matches |pattern|
  // case-sensitively.
  const RE2* GetRegEx(std::string_view pattern);

#ifdef UNIT_TEST
  // Returns true if the compiled regular expression corresponding to |pattern|
  // is cached.
  bool IsRegExCachedForTesting(std::string_view pattern) {
    return regex_map_.count(pattern) > 0;
  }
#endif

 private:
  Re2RegExCache();

  // Since the constructor is private, |base::NoDestructor| must be friend to be
  // allowed to construct the cache.
  friend class base::NoDestructor<Re2RegExCache>;

  // Stores a compiled regular expression keyed by its corresponding |pattern|.
  std::map<std::string, std::unique_ptr<const RE2>, std::less<>> regex_map_;

  // A lock to prevent concurrent access to the map.
  base::Lock lock_;
};

// Returns true if |name| has the characteristics of a Chinese, Japanese or
// Korean name:
// * It must only contain CJK characters with at most one separator in between.
bool HasCjkNameCharacteristics(const std::string& name);

// Returns true if |name| has one of the characteristics of an Hispanic/Latinx
// name:
// * Name contains a very common Hispanic/Latinx surname.
// * Name uses a surname conjunction.
bool HasHispanicLatinxNameCharacteristics(const std::string& name);

// Returns true if |middle_name| has the characteristics of a containing only
// initials:
// * The string contains only upper case letters that may be preceded by a
// point.
// * Between each letter, there can be a space or a hyphen.
bool HasMiddleNameInitialsCharacteristics(const std::string& middle_name);

// Reduces a name to the initials in upper case.
// Example: George walker -> GW, Hans-Peter -> HP
std::u16string ReduceToInitials(const std::u16string& value);

// Parses |value| with an regular expression defined by |pattern|.
// If the expression is fully matched, returns the matching results, keyed by
// the name of the capture group with the captured substrings as the value.
// Otherwise returns `nullopt`.
std::optional<base::flat_map<std::string, std::string>>
ParseValueByRegularExpression(const std::string& value, const RE2* regex);

// Same as above, but accepts pattern instead of a compiled regular expression.
std::optional<base::flat_map<std::string, std::string>>
ParseValueByRegularExpression(const std::string& value,
                              const std::string& pattern);

// Returns a compiled case sensitive regular expression for |pattern|.
std::unique_ptr<const RE2> BuildRegExFromPattern(std::string_view pattern);

// Returns true if |value| can be matched by the enumuerated RegEx |regex|.
bool IsPartialMatch(const std::string& value, RegEx regex);

// Returns true if |value| can be matched with |pattern|.
bool IsPartialMatch(const std::string& value, const std::string& pattern);

// Same as above, but accepts a compiled regular expression instead of the
// pattern.
bool IsPartialMatch(const std::string& value, const RE2* expression);

// Returns a vector that contains all partial matches of |pattern| in |value|;
std::vector<std::string> GetAllPartialMatches(const std::string& value,
                                              const std::string& pattern);

// Extracts all placeholders of the format ${PLACEHOLDER} in |value|.
std::vector<std::string> ExtractAllPlaceholders(const std::string& value);

// Returns |value| as a placeholder token: ${value}.
std::string GetPlaceholderToken(std::string_view value);

// Returns a named capture group created by the concatenation of the
// string_views in |pattern_span_initializer_list|. The group is named by the
// string representation of |type| and respects |options|.
std::string CaptureTypeWithPattern(
    const FieldType& type,
    std::initializer_list<std::string_view> pattern_span_initializer_list,
    const CaptureOptions& options);

// Same as |CaptureTypeWithPattern(type, pattern_span_initializer_list,
// options)| but uses default options.
std::string CaptureTypeWithPattern(
    const FieldType& type,
    std::initializer_list<std::string_view> pattern_span_initializer_list);

// A pattern that is used to capture tokens that are not supposed to be
// associated into a type.
std::string NoCapturePattern(const std::string& pattern,
                             const CaptureOptions& options = CaptureOptions());

// Returns a capture group named by the string representation of |type| that
// matches |pattern| with an additional uncaptured |prefix_pattern| and
// |suffix_pattern|.
std::string CaptureTypeWithAffixedPattern(
    const FieldType& type,
    const std::string& prefix_pattern,
    const std::string& pattern,
    const std::string& suffix_pattern,
    const CaptureOptions& options = CaptureOptions());

// Convenience wrapper for |CaptureTypeWithAffixedPattern()| with an empty
// |suffix_pattern|.
std::string CaptureTypeWithPrefixedPattern(
    const FieldType& type,
    const std::string& prefix_pattern,
    const std::string& pattern,
    const CaptureOptions& options = CaptureOptions());

// Convenience wrapper for |CaptureTypeWithAffixedPattern()| with an empty
// |prefix_pattern|.
std::string CaptureTypeWithSuffixedPattern(
    const FieldType& type,
    const std::string& pattern,
    const std::string& suffix_pattern,
    const CaptureOptions& options = CaptureOptions());

// Convenience wrapper for |CaptureTypeWithAffixedPattern()| with an empty
// |prefix_pattern| and |suffix_pattern|.
std::string CaptureTypeWithPattern(
    const FieldType& type,
    const std::string& pattern,
    const CaptureOptions options = CaptureOptions());

// Normalizes and rewrites `text` using the rules for `country_code`.
// If `country_code` is empty, it defaults to US.
std::u16string NormalizeAndRewrite(const AddressCountryCode& country_code,
                                   const std::u16string& text,
                                   bool keep_white_space);

// Collapses white spaces and line breaks, converts the string to lower case and
// removes diacritics.
// If |keep_white_spaces| is true, white spaces are collapsed. Otherwise,
// white spaces are completely removed.
std::u16string NormalizeValue(std::u16string_view value,
                              bool keep_white_space = true);

// Returns true of both vectors contain the same tokens in the same order.
bool AreSortedTokensEqual(const std::vector<AddressToken>& first,
                          const std::vector<AddressToken>& second);

// Returns true if both strings contain the same tokens after normalization.
bool AreStringTokenEquivalent(const std::u16string& one,
                              const std::u16string& other);

// Returns true if all tokens from the first string are contained in the set of
// tokens from the second string.
bool AreStringTokenCompatible(const std::u16string& first,
                              const std::u16string& second);

// Returns a sorted vector containing the tokens of |value| after |value| was
// canonicalized. |value| is tokenized by splitting it by white spaces and
// commas.
std::vector<AddressToken> TokenizeValue(const std::u16string value);

// Compares two vectors of sorted AddressTokens and returns the
// SortedTokenComparisonResult;
SortedTokenComparisonResult CompareSortedTokens(
    const std::vector<AddressToken>& first,
    const std::vector<AddressToken>& second);

// Convenience wrapper to supply untokenized strings.
SortedTokenComparisonResult CompareSortedTokens(const std::u16string& first,
                                                const std::u16string& second);

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_UTILS_H_
