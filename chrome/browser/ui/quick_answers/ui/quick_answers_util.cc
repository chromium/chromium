// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"

#include "base/strings/escape.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/flex_layout.h"

namespace {

// Spacing between labels in the horizontal elements view.
constexpr int kLabelSpacingDip = 2;

// Google search link.
constexpr char kGoogleSearchUrlPrefix[] = "https://www.google.com/search?q=";
constexpr char kGoogleTranslateUrlTemplate[] =
    "https://translate.google.com/?sl=auto&tl=%s&text=%s&op=translate";
constexpr char kTranslationQueryPrefix[] = "Translate:";

}  // namespace

namespace quick_answers {

using views::View;

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

View* AddHorizontalUiElements(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements,
    View* container) {
  auto* labels_container = container->AddChildView(std::make_unique<View>());
  auto* layout =
      labels_container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, 0, kLabelSpacingDip));

  for (const auto& element : elements) {
    switch (element->type) {
      case QuickAnswerUiElementType::kText:
        labels_container->AddChildView(std::make_unique<QuickAnswersTextLabel>(
            *static_cast<QuickAnswerText*>(element.get())));
        break;
      case QuickAnswerUiElementType::kImage:
        // TODO(yanxiao): Add image view
        break;
      case QuickAnswerUiElementType::kUnknown:
        LOG(ERROR) << "Trying to add an unknown QuickAnswerUiElement.";
        break;
    }
  }

  return labels_container;
}

GURL GetDetailsUrlForQuery(const std::string& query) {
  // TODO(b/240619915): Refactor so that we can access the request metadata
  // instead of just the query itself.
  if (base::StartsWith(query, kTranslationQueryPrefix)) {
    auto query_text = base::EscapeUrlEncodedData(
        query.substr(strlen(kTranslationQueryPrefix)), /*use_plus=*/true);
    auto device_language =
        l10n_util::GetLanguage(QuickAnswersState::Get()->application_locale());
    auto translate_url =
        base::StringPrintf(kGoogleTranslateUrlTemplate, device_language.c_str(),
                           query_text.c_str());
    return GURL(translate_url);
  } else {
    return GURL(kGoogleSearchUrlPrefix +
                base::EscapeUrlEncodedData(query, /*use_plus=*/true));
  }
}

}  // namespace quick_answers
