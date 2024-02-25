// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "chromeos/components/quick_answers/utils/unit_converter.h"

namespace quick_answers {
namespace {

using base::Value;

constexpr double kPreferredRatioRange = 100;
constexpr int kMaxAlternativeUnitsNumber = 4;

std::optional<ConversionRule> CreateConversionRule(
    const Value::Dict& unit,
    const std::string& category) {
  const std::string* unit_name = unit.FindStringByDottedPath(kNamePath);
  if (!unit_name) {
    return std::nullopt;
  }

  return ConversionRule::Create(category, *unit_name,
                                unit.FindDouble(kConversionToSiAPath),
                                unit.FindDouble(kConversionToSiBPath),
                                unit.FindDouble(kConversionToSiCPath));
}

std::vector<UnitConversion> ParseAlternativeUnitConversions(
    const Value::Dict& result,
    const UnitConversion& unit_conversion) {
  std::vector<UnitConversion> alternative_unit_conversions;

  const Value::List* rule = result.FindListByDottedPath(kRuleSetPath);
  if (!rule) {
    return alternative_unit_conversions;
  }

  UnitConverter converter(*rule);
  const Value::List* possible_units =
      converter.GetPossibleUnitsForCategory(unit_conversion.category());
  if (!possible_units) {
    return alternative_unit_conversions;
  }

  for (const Value& unit : *possible_units) {
    const Value::Dict& unit_dict = unit.GetDict();
    const std::string* unit_name = unit_dict.FindStringByDottedPath(kNamePath);
    // Filter out the source and destination units.
    if (!unit_name || *unit_name == unit_conversion.source_rule().unit_name() ||
        *unit_name == unit_conversion.dest_rule().unit_name()) {
      continue;
    }

    std::optional<ConversionRule> alternative_dest_rule =
        CreateConversionRule(unit_dict, unit_conversion.category());
    if (!alternative_dest_rule) {
      continue;
    }

    std::optional<UnitConversion> alternative_unit_conversion =
        UnitConversion::Create(unit_conversion.source_rule(),
                               alternative_dest_rule.value());
    if (!alternative_unit_conversion) {
      continue;
    }

    alternative_unit_conversions.push_back(alternative_unit_conversion.value());
  }

  // Sort |alternative_unit_conversions| from lowest to highest linear
  // conversion rates, then limit the vector size to
  // |kMaxAlternativeUnitsNumber| results.
  base::ranges::sort(alternative_unit_conversions);
  if (alternative_unit_conversions.size() > kMaxAlternativeUnitsNumber) {
    alternative_unit_conversions.erase(
        alternative_unit_conversions.begin() + kMaxAlternativeUnitsNumber,
        alternative_unit_conversions.end());
  }

  return alternative_unit_conversions;
}

}  // namespace

// Extract |quick_answer| from unit conversion result.
bool UnitConversionResultParser::Parse(const Value::Dict& result,
                                       QuickAnswer* quick_answer) {
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult(result);
  if (!structured_result) {
    return false;
  }

  return PopulateQuickAnswer(*structured_result, quick_answer);
}

std::unique_ptr<StructuredResult>
UnitConversionResultParser::ParseInStructuredResult(const Value::Dict& result) {
  std::unique_ptr<UnitConversionResult> unit_conversion_result =
      std::make_unique<UnitConversionResult>();

  const std::string* category =
      result.FindStringByDottedPath(kResultCategoryPath);
  if (!category) {
    LOG(ERROR) << "Failed to get the category for the conversion.";
    return nullptr;
  }
  unit_conversion_result->category = *category;

  const std::string* source_text =
      result.FindStringByDottedPath(kSourceTextPath);
  if (!source_text) {
    LOG(ERROR) << "Failed to get the source amount and unit text.";
    return nullptr;
  }
  unit_conversion_result->source_text = *source_text;

  const std::optional<double> source_amount =
      result.FindDoubleByDottedPath(kSourceAmountPath);
  if (!source_amount) {
    LOG(ERROR) << "Failed to get the source amount.";
    return nullptr;
  }
  unit_conversion_result->source_amount = source_amount.value();

  std::optional<ConversionRule> source_rule;
  const Value::Dict* source_unit = result.FindDictByDottedPath(kSourceUnitPath);
  if (source_unit) {
    source_rule = CreateConversionRule(*source_unit, *category);
  }

  std::optional<ConversionRule> dest_rule;
  const Value::Dict* dest_unit = result.FindDictByDottedPath(kDestUnitPath);
  if (dest_unit) {
    dest_rule = CreateConversionRule(*dest_unit, *category);
  }

  std::string result_string;

  // If the conversion ratio is not within the preferred range, try to find a
  // better destination unit type.
  // This only works if we have a valid source unit.
  if (source_unit) {
    const std::optional<double> dest_amount =
        result.FindDoubleByDottedPath(kDestAmountPath);
    const std::optional<double> ratio = GetRatio(source_amount, dest_amount);

    if (ratio && ratio.value() > kPreferredRatioRange) {
      const Value::List* rule = result.FindListByDottedPath(kRuleSetPath);
      if (rule) {
        UnitConverter converter(*rule);
        dest_unit =
            converter.FindProperDestinationUnit(*source_unit, ratio.value());
        if (dest_unit) {
          result_string = converter.Convert(source_amount.value(), *source_unit,
                                            *dest_unit);
          // If a valid result is found, update the `dest_rule` value
          // accordingly to get the ConversionRule for the new `dest_unit`.
          if (!result_string.empty()) {
            dest_rule = CreateConversionRule(*dest_unit, *category);
          }
        }
      }
    }
  }

  // Fallback to the existing result.
  if (result_string.empty()) {
    const std::string* dest_text = result.FindStringByDottedPath(kDestTextPath);
    if (!dest_text) {
      LOG(ERROR) << "Failed to get the conversion result.";
      return nullptr;
    }
    result_string = *dest_text;
  }
  unit_conversion_result->result_text = result_string;

  // Both source and destination unit ConversionRules must be valid for there to
  // be valid unit conversions.
  if (source_rule && dest_rule) {
    std::optional<UnitConversion> unit_conversion =
        UnitConversion::Create(source_rule.value(), dest_rule.value());
    unit_conversion_result->source_to_dest_unit_conversion = unit_conversion;

    if (unit_conversion) {
      std::vector<UnitConversion> alternative_unit_conversions_list =
          ParseAlternativeUnitConversions(result, unit_conversion.value());
      unit_conversion_result->alternative_unit_conversions_list =
          alternative_unit_conversions_list;
    }
  }

  std::unique_ptr<StructuredResult> structured_result =
      std::make_unique<StructuredResult>();
  structured_result->unit_conversion_result = std::move(unit_conversion_result);

  return structured_result;
}

bool UnitConversionResultParser::PopulateQuickAnswer(
    const StructuredResult& structured_result,
    QuickAnswer* quick_answer) {
  UnitConversionResult* unit_conversion_result =
      structured_result.unit_conversion_result.get();
  if (!unit_conversion_result) {
    DLOG(ERROR) << "Unable to find unit_conversion_result.";
    return false;
  }

  quick_answer->result_type = ResultType::kUnitConversionResult;
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(
          unit_conversion_result->result_text));

  return true;
}

bool UnitConversionResultParser::SupportsNewInterface() const {
  return true;
}

}  // namespace quick_answers
