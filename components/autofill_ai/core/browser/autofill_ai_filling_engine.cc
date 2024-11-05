// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_filling_engine.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace autofill_ai {

AutofillAiFillingEngine::Prediction::Prediction(std::u16string value,
                                                std::u16string label,
                                                bool is_focusable)
    : Prediction(std::move(value),
                 std::move(label),
                 is_focusable,
                 std::nullopt) {}

AutofillAiFillingEngine::Prediction::Prediction(
    std::u16string value,
    std::u16string label,
    bool is_focusable,
    std::optional<std::u16string> select_option_text)
    : value(std::move(value)),
      label(std::move(label)),
      is_focusable(is_focusable),
      select_option_text(std::move(select_option_text)) {}

AutofillAiFillingEngine::Prediction::Prediction(const Prediction& other) =
    default;
AutofillAiFillingEngine::Prediction::~Prediction() = default;

}  // namespace autofill_ai
