// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

#include <string>

#include "base/logging.h"
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
  std::string result_string;

  const auto src_amount = result.FindDoubleByDottedPath(kSourceAmountPath);
  const auto dst_amount = result.FindDoubleByDottedPath(kDestAmountPath);
  // If the conversion ratio is not within the preferred range, try to find a
  // better destination unit type.
  if (src_amount.has_value() && dst_amount.has_value()) {
    const auto ratio = GetRatio(src_amount.value(), dst_amount.value());
    if (ratio.has_value() && ratio.value() > kPreferredRatioRange) {
      const auto* rule = result.FindListByDottedPath(kRuleSetPath);
      if (rule) {
        UnitConverter converter(*rule);

        const auto* src_unit = result.FindDictByDottedPath(kSourceUnitPath);
        if (src_unit) {
          const auto* dst_unit =
              converter.FindProperDestinationUnit(*src_unit, ratio.value());

          if (dst_unit) {
            result_string =
                converter.Convert(src_amount.value(), *src_unit, *dst_unit);
          }
        }
      }
    }
  }

  // Fallback to the existing result.
  if (result_string.empty()) {
    auto* dest = result.FindStringByDottedPath(kDestTextPath);
    if (!dest) {
      LOG(ERROR) << "Failed to get the conversion result.";
      return false;
    }
    result_string = *dest;
  }

  quick_answer->result_type = ResultType::kUnitConversionResult;
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(result_string));

  return true;
}

}  // namespace quick_answers
