// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {

// The combined width of the read and copy buttons and all their
// associated margins.
constexpr int kReadAndCopyButtonsWidth = 64;

}  // namespace

namespace quick_answers {

// RichAnswersTranslationView
// -----------------------------------------------------------

RichAnswersTranslationView::RichAnswersTranslationView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const TranslationResult& translation_result)
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
  content_view_ = GetContentView();

  // Source language.
  AddLanguageTitle(translation_result_.source_locale, /*is_header_view=*/true);

  // Text to translate.
  AddLanguageText(translation_result_.text_to_translate,
                  /*may_append_buttons=*/false);

  // Separator.
  content_view_->AddChildView(CreateSeparatorView());

  // Target language.
  AddLanguageTitle(translation_result_.target_locale, /*is_header_view=*/false);

  // Translated text.
  auto* translated_text_view = AddLanguageText(
      translation_result_.translated_text, /*may_append_buttons=*/true);

  // Read and copy buttons.
  AddReadAndCopyButtons(translated_text_view);
}

void RichAnswersTranslationView::AddLanguageTitle(const std::string& locale,
                                                  bool is_header_view) {
  auto locale_name = l10n_util::GetDisplayNameForLocale(
      locale, translation_result_.target_locale, true);

  if (is_header_view) {
    AddHeaderViewsTo(content_view_, base::UTF16ToUTF8(locale_name));
  } else {
    AddFillLayoutChildView(
        content_view_,
        QuickAnswersTextLabel::CreateLabelWithStyle(
            base::UTF16ToUTF8(locale_name),
            GetFontList(TypographyToken::kCrosButton2), kContentHeaderWidth,
            /*is_multi_line=*/false, cros_tokens::kCrosSysSecondary));
  }
}

views::FlexLayoutView* RichAnswersTranslationView::AddLanguageText(
    const std::string& language_text,
    bool may_append_buttons) {
  auto* container_view = AddFillLayoutChildView(content_view_);
  auto* language_text_view = container_view->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .Build());
  std::unique_ptr<QuickAnswersTextLabel> language_text_label =
      QuickAnswersTextLabel::CreateLabelWithStyle(
          language_text, GetFontList(TypographyToken::kCrosTitle1),
          kContentTextWidth,
          /*is_multi_line=*/true, cros_tokens::kCrosSysOnSurface);
  // If appending the read and copy buttons is an option, then check if
  // the button views can fit on a single line with the text label.
  if (may_append_buttons &&
      (language_text_label->CalculatePreferredSize().width() +
       kReadAndCopyButtonsWidth) <= kContentTextWidth) {
    should_append_buttons_ = true;
  }
  language_text_view->AddChildView(std::move(language_text_label));

  return language_text_view;
}

void RichAnswersTranslationView::AddReadAndCopyButtons(
    views::FlexLayoutView* translated_text_view) {
  views::View* container_view;

  // If |should_append_buttons_| is true, then append the buttons to the same
  // view as the translated text. Otherwise, add the buttons on a separate line
  // underneath.
  if (should_append_buttons_) {
    translated_text_view->SetDefault(views::kMarginsKey,
                                     kViewHorizontalSpacingMargins);
    container_view = translated_text_view;
  } else {
    container_view = AddFillLayoutChildView(content_view_);
  }

  auto* button_views =
      container_view->AddChildView(CreateHorizontalLayoutView());

  // Setup an invisible web view to play TTS audio.
  tts_audio_web_view_ = container_view->AddChildView(
      std::make_unique<views::WebView>(ProfileManager::GetActiveUserProfile()));
  tts_audio_web_view_->SetVisible(false);

  // Read button.
  base::RepeatingClosure read_closure = base::BindRepeating(
      &RichAnswersTranslationView::OnReadButtonPressed,
      weak_factory_.GetWeakPtr(), translation_result_.translated_text,
      translation_result_.target_locale);
  ui::ImageModel read_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kVolumeUpIcon, cros_tokens::kCrosSysOnSurface,
      /*icon_size=*/kRichAnswersIconSizeDip);
  button_views->AddChildView(CreateImageButtonView(
      read_closure, read_image_model, cros_tokens::kCrosSysHoverOnSubtle,
      l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_PHONETICS_BUTTON_TOOLTIP_TEXT)));

  // Copy button.
  base::RepeatingClosure copy_closure = base::BindRepeating(
      &RichAnswersTranslationView::OnCopyButtonPressed,
      weak_factory_.GetWeakPtr(), translation_result_.translated_text);
  ui::ImageModel copy_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kContentCopyIcon, cros_tokens::kCrosSysOnSurface,
      /*icon_size=*/kRichAnswersIconSizeDip);
  button_views->AddChildView(CreateImageButtonView(
      copy_closure, copy_image_model, cros_tokens::kCrosSysHoverOnSubtle,
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_COPY_BUTTON_TOOLTIP_TEXT)));
}

void RichAnswersTranslationView::OnReadButtonPressed(
    const std::string& read_text,
    const std::string& locale) {
  GenerateTTSAudio(tts_audio_web_view_->GetBrowserContext(), read_text, locale);
}

void RichAnswersTranslationView::OnCopyButtonPressed(
    const std::string& copy_text) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  writer.WriteText(base::UTF8ToUTF16(copy_text));
}

BEGIN_METADATA(RichAnswersTranslationView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
