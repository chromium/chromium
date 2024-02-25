// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/unit_converter.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"

namespace quick_answers {
namespace {

using base::Value;

bool IsLinearFormula(const std::optional<double> rate_a) {
  return rate_a.has_value() && rate_a.value() != kInvalidRateTermValue;
}

}  // namespace

UnitConverter::UnitConverter(const Value::List& rule_set)
    : rule_set_(rule_set) {}

UnitConverter::~UnitConverter() = default;

const std::string UnitConverter::Convert(const double src_value,
                                         const Value::Dict& src_unit,
                                         const Value::Dict& dst_unit) {
  // Validate the inputs.
  const auto* src_name = src_unit.FindStringByDottedPath(kNamePath);
  const auto src_rate_a = src_unit.FindDoubleByDottedPath(kConversionToSiAPath);
  if (!src_name || !IsLinearFormula(src_rate_a)) {
    return std::string();
  }
  const auto* dst_name = dst_unit.FindStringByDottedPath(kNamePath);
  const auto dst_rate_a = dst_unit.FindDoubleByDottedPath(kConversionToSiAPath);
  if (!dst_name || !IsLinearFormula(dst_rate_a)) {
    return std::string();
  }

  const double result_value =
      (src_rate_a.value() / dst_rate_a.value()) * src_value;

  return BuildUnitConversionResultText(
      BuildRoundedUnitAmountDisplayText(result_value),
      GetUnitDisplayText(*dst_name));
}

const Value::Dict* UnitConverter::FindProperDestinationUnit(
    const Value::Dict& src_unit,
    const double preferred_range) {
  const auto* src_category = src_unit.FindStringByDottedPath(kCategoryPath);
  const auto* src_name = src_unit.FindStringByDottedPath(kNamePath);
  const auto src_rate_a = src_unit.FindDoubleByDottedPath(kConversionToSiAPath);
  // Make sure the input source unit is valid.
  if (!src_category || !src_name || !IsLinearFormula(src_rate_a))
    return nullptr;

  const auto* units = GetPossibleUnitsForCategory(*src_category);
  if (!units)
    return nullptr;

  // Find the unit with closest linear conversion rate within the preferred
  // range. If no proper unit found, return nullptr.
  const Value::Dict* dst_unit = nullptr;
  double min_rate = preferred_range;
  for (const Value& unit_value : *units) {
    const Value::Dict& unit = unit_value.GetDict();
    const auto* name = unit.FindStringByDottedPath(kNamePath);
    const auto rate_a = unit.FindDoubleByDottedPath(kConversionToSiAPath);
    if (*name == *src_name || !IsLinearFormula(rate_a)) {
      continue;
    }
    auto rate = GetRatio(rate_a.value(), src_rate_a.value());
    if (rate.has_value() && rate.value() < min_rate) {
      min_rate = rate.value();
      dst_unit = &unit;
    }
  }

  return dst_unit;
}

const Value::Dict* UnitConverter::GetConversionForCategory(
    const std::string& target_category) {
  for (const Value& conversion : *rule_set_) {
    const Value::Dict& conversion_dict = conversion.GetDict();
    const auto* category =
        conversion_dict.FindStringByDottedPath(kCategoryPath);
    if (category && *category == target_category)
      return &conversion_dict;
  }
  return nullptr;
}

const Value::List* UnitConverter::GetPossibleUnitsForCategory(
    const std::string& target_category) {
  // Get the list of conversion rate for the category.
  const auto* conversion = GetConversionForCategory(target_category);
  if (!conversion)
    return nullptr;

  return conversion->FindListByDottedPath(kUnitsPath);
}

}  // namespace quick_answers
