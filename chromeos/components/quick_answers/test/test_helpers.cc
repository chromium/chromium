// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"

namespace quick_answers {

std::string GetQuickAnswerTextForTesting(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements) {
  std::string text = "";

  for (const auto& element : elements) {
    switch (element->type) {
      case QuickAnswerUiElementType::kText:
        text += base::UTF16ToUTF8(
            static_cast<QuickAnswerText*>(element.get())->text);
        break;
      default:
        break;
    }
  }

  return UnescapeStringForHTML(text);
}

base::Value::Dict CreateUnit(const std::string& name,
                             double rate_a,
                             double rate_b,
                             const std::string& category,
                             double rate_c) {
  base::Value::Dict unit;
  unit.Set(kNamePath, name);

  // Since the vast majority of conversion rates involve a |rate_a| term, we
  // will assume that |kConversionToSiAPath| will always be set (even if only to
  // |kInvalidRateTermValue|).
  // This behavior is used for certain test cases in unit_converter_unittest.cc
  unit.Set(kConversionToSiAPath, rate_a);

  if (rate_b != kInvalidRateTermValue) {
    unit.Set(kConversionToSiBPath, rate_b);
  }

  if (rate_c != kInvalidRateTermValue) {
    unit.Set(kConversionToSiCPath, rate_c);
  }

  if (!category.empty()) {
    unit.Set(kCategoryPath, category);
  }

  return unit;
}

MockQuickAnswersDelegate::MockQuickAnswersDelegate() = default;
MockQuickAnswersDelegate::~MockQuickAnswersDelegate() = default;

MockResultLoaderDelegate::MockResultLoaderDelegate() = default;
MockResultLoaderDelegate::~MockResultLoaderDelegate() = default;

}  // namespace quick_answers
