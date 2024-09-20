// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_

#include "base/check_op.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// The sources from which strings are matched: the field's label or its name or
// id attribute value.
//
// For example, in
// <label for="mobile">Cellphone number:</label> <input type="tel" id="mobile">
// the kLabel is "Cellphone number" and the kName is "mobile".
enum class MatchAttribute { kLabel, kName, kMaxValue = kName };

// A pair of sets of MatchAttributes and FormControlTypes.
struct MatchParams {
  inline constexpr MatchParams(DenseSet<MatchAttribute> attributes,
                               DenseSet<FormControlType> field_types);
  inline constexpr MatchParams(const MatchParams&);
  inline constexpr MatchParams& operator=(const MatchParams&);
  inline constexpr MatchParams(MatchParams&&);
  inline constexpr MatchParams& operator=(MatchParams&&);

  DenseSet<MatchAttribute> attributes;
  DenseSet<FormControlType> field_types;
};

inline constexpr MatchParams::MatchParams(DenseSet<MatchAttribute> attributes,
                                          DenseSet<FormControlType> field_types)
    : attributes(attributes), field_types(field_types) {}
inline constexpr MatchParams::MatchParams(const MatchParams&) = default;
inline constexpr MatchParams& MatchParams::operator=(const MatchParams&) =
    default;
inline constexpr MatchParams::MatchParams(MatchParams&&) = default;
inline constexpr MatchParams& MatchParams::operator=(MatchParams&&) = default;

// By default match label and name for <input type="text"> elements.
template <FormControlType... additional_match_field_types>
constexpr MatchParams kDefaultMatchParamsWith{
    {MatchAttribute::kLabel, MatchAttribute::kName},
    {FormControlType::kInputText, additional_match_field_types...}};

constexpr MatchParams kDefaultMatchParams = kDefaultMatchParamsWith<>;

// References to `base::Feature`s that are used to gate regexes. Defined as a
// separate enum to reduce the memory overhead. They are used to annotate
// `MatchingPattern`s, so the parsing logic can check at runtime which patterns
// to apply.
enum class RegexFeature : uint8_t {
  // This entry only exists to ensure that the enum never becomes empty as
  // features are added and removed.
  kUnusedDummyFeature = 0,
  kAutofillGreekRegexes = 1,
  kMaxValue = kAutofillGreekRegexes
};

// Returns a `DenseSet` containing all `RegexFeature`s whose corresponding
// `base::Feature` is enabled.
DenseSet<RegexFeature> GetActiveRegexFeatures();

// Each `MatchingPattern` can either be applied unconditionally or only if a
// certain `RegexFeature` is in a certain state. This is a memory-optimized
// representation of this state. Conceptually, it's a
// std::optional<std::pair<RegexFeature, bool>>.
class OptionalRegexFeatureWithState {
 public:
  constexpr OptionalRegexFeatureWithState()
      : feature_(RegexFeature::kUnusedDummyFeature),
        enabled_(false),
        engaged_(false) {}
  constexpr OptionalRegexFeatureWithState(RegexFeature feature, bool enabled)
      : feature_(feature), enabled_(enabled), engaged_(true) {
    CHECK_NE(feature, RegexFeature::kUnusedDummyFeature);
  }

  constexpr bool has_value() const { return engaged_; }
  constexpr RegexFeature feature() const {
    CHECK(has_value());
    return feature_;
  }
  constexpr bool enabled() const {
    CHECK(has_value());
    return enabled_;
  }

 private:
  static_assert(static_cast<uint8_t>(RegexFeature::kMaxValue) < (1 << 6));

  const RegexFeature feature_ : 6;
  const uint8_t enabled_ : 1;
  const uint8_t engaged_ : 1;
};

// Structure for a better organization of data and regular expressions
// for autofill regex_constants. In the future, to implement faster
// changes without global updates also for having a quick possibility
// to recognize incorrect matches.
//
// We pack this struct to minimize memory consumption of the built-in array of
// MatchingPatterns (see GetMatchPatterns()), which holds several hundred
// objects.
// Using packed DenseSets reduces the size of the struct by 40 to 24 on 64 bit
// platforms, and from 24 to 20 bytes on 32 bit platforms.
struct MatchingPattern {
  const char16_t* positive_pattern;
  const char16_t* negative_pattern;
  const DenseSet<MatchAttribute> match_field_attributes;
  const DenseSet<FormControlType> form_control_types;
  const OptionalRegexFeatureWithState feature;

  // Returns true if the pattern should be applied based on the `feature` and
  // the `active_features`.
  bool IsActive(DenseSet<RegexFeature> active_features) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_
