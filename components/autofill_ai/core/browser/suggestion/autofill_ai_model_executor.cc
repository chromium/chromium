// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace autofill_ai {

AutofillAiModelExecutor::Prediction::Prediction(std::u16string value,
                                                std::u16string label,
                                                bool is_focusable)
    : Prediction(std::move(value),
                 std::move(label),
                 is_focusable,
                 std::nullopt) {}

AutofillAiModelExecutor::Prediction::Prediction(
    std::u16string value,
    std::u16string label,
    bool is_focusable,
    std::optional<std::u16string> select_option_text)
    : value(std::move(value)),
      label(std::move(label)),
      is_focusable(is_focusable),
      select_option_text(std::move(select_option_text)) {}

AutofillAiModelExecutor::Prediction::Prediction(const Prediction& other) =
    default;
AutofillAiModelExecutor::Prediction::~Prediction() = default;

}  // namespace autofill_ai
