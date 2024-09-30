// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace autofill_prediction_improvements {

AutofillPredictionImprovementsFillingEngine::Prediction::Prediction(
    std::u16string value,
    std::u16string label)
    : Prediction(std::move(value), std::move(label), std::nullopt) {}

AutofillPredictionImprovementsFillingEngine::Prediction::Prediction(
    std::u16string value,
    std::u16string label,
    std::optional<std::u16string> select_option_text)
    : value(std::move(value)),
      label(std::move(label)),
      select_option_text(select_option_text) {}

AutofillPredictionImprovementsFillingEngine::Prediction::Prediction(
    const Prediction& other) = default;
AutofillPredictionImprovementsFillingEngine::Prediction::~Prediction() =
    default;

// For tests to readably print an instance of this struct.
void PrintTo(
    const AutofillPredictionImprovementsFillingEngine::Prediction& prediction,
    std::ostream* os) {
  *os << "Prediction { " << ".value = \"" << base::UTF16ToUTF8(prediction.value)
      << "\", " << ".label = \"" << base::UTF16ToUTF8(prediction.label)
      << "\", " << ".select_option_text = "
      << (prediction.select_option_text
              ? base::StrCat({"\"",
                              base::UTF16ToUTF8(*prediction.select_option_text),
                              "\""})
              : "std::nullopt")
      << " " << "}";
}

}  // namespace autofill_prediction_improvements
