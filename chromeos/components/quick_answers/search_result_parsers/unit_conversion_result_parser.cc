// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "chromeos/components/quick_answers/utils/unit_converter.h"

namespace quick_answers {
namespace {

using base::Value;

constexpr double kPreferredRatioRange = 100;

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
UnitConversionResultParser::ParseInStructuredResult(
    const base::Value::Dict& result) {
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
    LOG(ERROR) << "Failed to get the source amount and unit.";
    return nullptr;
  }
  unit_conversion_result->source_text = *source_text;

  double source_to_standard_conversion_rate = kInvalidRateValue;
  const base::Value::Dict* source_unit =
      result.FindDictByDottedPath(kSourceUnitPath);
  if (source_unit) {
    std::optional<double> conversion_to_standard_unit_rate =
        source_unit->FindDouble(kConversionToSiAPath);
    if (conversion_to_standard_unit_rate) {
      source_to_standard_conversion_rate =
          conversion_to_standard_unit_rate.value();
    }
  }

  double dest_to_standard_conversion_rate = kInvalidRateValue;
  const base::Value::Dict* dest_unit =
      result.FindDictByDottedPath(kDestUnitPath);
  if (dest_unit) {
    std::optional<double> conversion_to_standard_unit_rate =
        dest_unit->FindDouble(kConversionToSiAPath);
    if (conversion_to_standard_unit_rate) {
      dest_to_standard_conversion_rate =
          conversion_to_standard_unit_rate.value();
    }
  }

  std::string result_string;

  // If the conversion ratio is not within the preferred range, try to find a
  // better destination unit type.
  // This only works if we have a valid source unit.
  if (source_unit) {
    const std::optional<double> source_amount =
        result.FindDoubleByDottedPath(kSourceAmountPath);
    const std::optional<double> dest_amount =
        result.FindDoubleByDottedPath(kDestAmountPath);
    const std::optional<double> ratio = GetRatio(source_amount, dest_amount);
    if (ratio && ratio.value() > kPreferredRatioRange) {
      const base::Value::List* rule = result.FindListByDottedPath(kRuleSetPath);
      if (rule) {
        UnitConverter converter(*rule);
        dest_unit =
            converter.FindProperDestinationUnit(*source_unit, ratio.value());
        if (dest_unit) {
          result_string = converter.Convert(source_amount.value(), *source_unit,
                                            *dest_unit);
          // If a valid result is found, update the
          // `dest_to_standard_conversion_rate` value accordingly to get the
          // conversion rate for the new `dest_unit`.
          if (!result_string.empty()) {
            std::optional<double> conversion_to_standard_unit_rate =
                dest_unit->FindDouble(kConversionToSiAPath);
            dest_to_standard_conversion_rate =
                conversion_to_standard_unit_rate.value_or(kInvalidRateValue);
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

  // Both conversion-to-SI-unit rates must be valid (non-zero) for there to be a
  // valid conversion rate between the source and destination units.
  if (source_to_standard_conversion_rate != kInvalidRateValue &&
      dest_to_standard_conversion_rate != kInvalidRateValue) {
    unit_conversion_result->standard_unit_conversion_rates =
        StandardUnitConversionRates(source_to_standard_conversion_rate,
                                    dest_to_standard_conversion_rate);
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
