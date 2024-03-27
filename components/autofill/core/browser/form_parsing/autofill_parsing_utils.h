// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_

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

// Structure for a better organization of data and regular expressions
// for autofill regex_constants. In the future, to implement faster
// changes without global updates also for having a quick possibility
// to recognize incorrect matches.
//
// We pack this struct to minimize memory consumption of the built-in array of
// MatchingPatterns (see GetMatchPatterns()), which holds several hundred
// objects.
// Using packed DenseSets reduces the size of the struct by 40 to 24 on 64 bit
// platforms, and from 20 to 16 bytes on 32 bit platforms.
struct MatchingPattern {
  const char16_t* positive_pattern;
  const char16_t* negative_pattern;
  const float positive_score = 1.1;
  const DenseSet<MatchAttribute> match_field_attributes;
  const DenseSet<FormControlType> form_control_types;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_
