// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_test_utils.h"

#include <ostream>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"

namespace autofill_ai {

// For tests to readably print an instance of this struct.
void PrintTo(const AutofillAiModelExecutor::Prediction& prediction,
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

}  // namespace autofill_ai
