// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

namespace quick_answers {

// RichAnswersTranslationView
// -----------------------------------------------------------

RichAnswersTranslationView::RichAnswersTranslationView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    TranslationResult& translation_result)
    : RichAnswersView(anchor_view_bounds,
                      controller,
                      ResultType::kTranslationResult),
      translation_result_(translation_result) {
  InitLayout();

  // TODO (b/274184294): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersTranslationView::~RichAnswersTranslationView() = default;

void RichAnswersTranslationView::InitLayout() {
  // TODO (b/265258270): Populate translation view contents.
  content_view_ = GetContentView();

  // Source language.
  AddLanguageTitle(translation_result_.source_locale, true);

  // Text to translate.
  AddLanguageText(translation_result_.text_to_translate);

  // Separator.
  content_view_->AddChildView(CreateSeparatorView());

  // Target language.
  AddLanguageTitle(translation_result_.target_locale, false);

  // Translated text.
  AddLanguageText(translation_result_.translated_text);
}

void RichAnswersTranslationView::AddLanguageTitle(const std::string& locale,
                                                  bool is_header_view) {
  auto locale_name = l10n_util::GetDisplayNameForLocale(
      locale, translation_result_.target_locale, true);

  if (is_header_view) {
    AddHeaderViewsTo(content_view_, base::UTF16ToUTF8(locale_name));
  } else {
    auto* second_header_view = AddFillLayoutChildView(
        content_view_,
        QuickAnswersTextLabel::CreateLabelWithStyle(
            base::UTF16ToUTF8(locale_name),
            GetFontList(TypographyToken::kCrosButton2), kContentHeaderWidth,
            /*is_multi_line=*/false, cros_tokens::kCrosSysSecondary));
    second_header_view->SetProperty(views::kMarginsKey, kUnderLineIndentation);
  }
}

void RichAnswersTranslationView::AddLanguageText(
    const std::string& language_text) {
  AddFillLayoutChildView(
      content_view_,
      QuickAnswersTextLabel::CreateLabelWithStyle(
          language_text, GetFontList(TypographyToken::kCrosTitle1),
          kContentTextWidth,
          /*is_multi_line=*/true, cros_tokens::kCrosSysOnSurface));
}

BEGIN_METADATA(RichAnswersTranslationView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
