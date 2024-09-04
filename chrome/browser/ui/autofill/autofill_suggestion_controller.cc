// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"

namespace autofill {

// static
AutofillSuggestionController::UiSessionId
AutofillSuggestionController::GenerateSuggestionUiSessionId() {
  static UiSessionId::Generator generator;
  return generator.GenerateNextId();
}

}  // namespace autofill
