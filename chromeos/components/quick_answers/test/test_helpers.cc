// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/test_helpers.h"

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

MockQuickAnswersDelegate::MockQuickAnswersDelegate() = default;
MockQuickAnswersDelegate::~MockQuickAnswersDelegate() = default;

MockResultLoaderDelegate::MockResultLoaderDelegate() = default;
MockResultLoaderDelegate::~MockResultLoaderDelegate() = default;

}  // namespace quick_answers
