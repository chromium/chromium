// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;

constexpr char kDestPath[] =
    "unitConversionResult.destination.valueAndUnit.rawText";

}  // namespace

// Extract |quick_answer| from unit conversion result.
bool UnitConversionResultParser::Parse(const Value* result,
                                       QuickAnswer* quick_answer) {
  const std::string* dest = result->FindStringPath(kDestPath);

  if (!dest) {
    LOG(ERROR) << "Can't find the destination value.";
    return false;
  }

  quick_answer->result_type = ResultType::kUnitConversionResult;
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(*dest));

  return true;
}

}  // namespace quick_answers
}  // namespace chromeos
