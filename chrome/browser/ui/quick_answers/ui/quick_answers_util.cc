// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"

namespace quick_answers {

const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type) {
  switch (result_type) {
    case ResultType::kDefinitionResult:
      return omnibox::kAnswerDictionaryIcon;
    case ResultType::kTranslationResult:
      return omnibox::kAnswerTranslationIcon;
    case ResultType::kUnitConversionResult:
      return omnibox::kAnswerCalculatorIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

}  // namespace quick_answers
