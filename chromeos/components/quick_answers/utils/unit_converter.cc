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

bool IsLinearFormula(const absl::optional<double> rate_a) {
  return rate_a.has_value() && rate_a.value() != 0;
}

}  // namespace

UnitConverter::UnitConverter(const Value& rule_set) : rule_set_(rule_set) {}

UnitConverter::~UnitConverter() = default;

const std::string UnitConverter::Convert(const double src_value,
                                         const base::Value& src_unit,
                                         const base::Value& dst_unit) {
  // Validate the inputs.
  const auto* src_name = src_unit.FindStringPath(kNamePath);
  const auto src_rate_a = src_unit.FindDoublePath(kConversionRateAPath);
  if (!src_name || !IsLinearFormula(src_rate_a)) {
    return std::string();
  }
  const auto* dst_name = dst_unit.FindStringPath(kNamePath);
  const auto dst_rate_a = dst_unit.FindDoublePath(kConversionRateAPath);
  if (!dst_name || !IsLinearFormula(dst_rate_a)) {
    return std::string();
  }

  const double result_value =
      (src_rate_a.value() / dst_rate_a.value()) * src_value;

  return BuildUnitConversionResultText(
      base::StringPrintf(kResultValueTemplate, result_value),
      GetUnitDisplayText(*dst_name));
}

const Value* UnitConverter::FindProperDestinationUnit(
    const Value& src_unit,
    const double preferred_range) {
  const auto* src_category = src_unit.FindStringPath(kCategoryPath);
  const auto* src_name = src_unit.FindStringPath(kNamePath);
  const auto src_rate_a = src_unit.FindDoublePath(kConversionRateAPath);
  // Make sure the input source unit is valid.
  if (!src_category || !src_name || !IsLinearFormula(src_rate_a))
    return nullptr;

  const auto* units = GetPossibleUnitsForCategory(*src_category);
  if (!units)
    return nullptr;

  // Find the unit with closest conversion rate within the preferred range. If
  // no proper unit found, return nullptr.
  const Value* dst_unit = nullptr;
  double min_rate = preferred_range;
  for (const Value& unit : units->GetList()) {
    const auto* name = unit.FindStringPath(kNamePath);
    const auto rate_a = unit.FindDoublePath(kConversionRateAPath);
    if (*name == *src_name || !rate_a.has_value() || rate_a.value() == 0)
      continue;
    auto rate = GetRatio(rate_a.value(), src_rate_a.value());
    if (rate.has_value() && rate.value() < min_rate) {
      min_rate = rate.value();
      dst_unit = &unit;
    }
  }

  return dst_unit;
}

const Value* UnitConverter::GetConversionForCategory(
    const std::string& target_category) {
  if (rule_set_.GetList().empty())
    return nullptr;
  for (const Value& conversion : rule_set_.GetList()) {
    const auto* category = conversion.FindStringPath(kCategoryPath);
    if (category && *category == target_category)
      return &conversion;
  }
  return nullptr;
}

const Value* UnitConverter::GetPossibleUnitsForCategory(
    const std::string& target_category) {
  // Get the list of conversion rate for the category.
  const auto* conversion = GetConversionForCategory(target_category);
  if (!conversion)
    return nullptr;

  return conversion->FindListPath(kUnitsPath);
}

}  // namespace quick_answers
