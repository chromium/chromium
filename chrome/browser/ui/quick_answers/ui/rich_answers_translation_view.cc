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
#include "ui/color/color_id.h"
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
                  /*maybe_append_buttons=*/false);

  // Separator.
  content_view_->AddChildView(CreateSeparatorView());

  // Target language.
  AddLanguageTitle(translation_result_.target_locale, /*is_header_view=*/false);

  // Translated text.
  views::FlexLayoutView* translated_text_view = AddLanguageText(
      translation_result_.translated_text, /*maybe_append_buttons=*/true);

  // Read and copy buttons.
  //
  // If `translated_text_view` is a valid container view, then append the button
  // views to it. Else, create a separate horizontal child view in
  // `content_view_` to contain the buttons.
  views::View* buttons_container_view;
  if (translated_text_view) {
    buttons_container_view = translated_text_view;
  } else {
    buttons_container_view =
        content_view_->AddChildView(CreateHorizontalBoxLayoutView());
  }
  AddReadAndCopyButtons(buttons_container_view);
}

void RichAnswersTranslationView::AddLanguageTitle(const std::string& locale,
                                                  bool is_header_view) {
  auto locale_name = l10n_util::GetDisplayNameForLocale(
      locale, translation_result_.target_locale, true);

  if (is_header_view) {
    AddHeaderViewsTo(content_view_, base::UTF16ToUTF8(locale_name));
    return;
  }

  content_view_->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      base::UTF16ToUTF8(locale_name),
      GetFontList(TypographyToken::kCrosButton2), kContentHeaderWidth,
      /*is_multi_line=*/false, ui::kColorSysSecondary));
}

views::FlexLayoutView* RichAnswersTranslationView::AddLanguageText(
    const std::string& language_text,
    bool maybe_append_buttons) {
  std::unique_ptr<QuickAnswersTextLabel> language_text_label =
      QuickAnswersTextLabel::CreateLabelWithStyle(
          language_text, GetFontList(TypographyToken::kCrosTitle1),
          kContentTextWidth,
          /*is_multi_line=*/true, ui::kColorSysOnSurface);

  // If appending the read and copy buttons is an option, then check if
  // the buttons can fit on a single line with the language text label.
  //
  // If there's enough space to append the buttons, return the container view
  // that the button views should be added to.
  if (maybe_append_buttons && (language_text_label->GetPreferredSize().width() +
                               kReadAndCopyButtonsWidth) <= kContentTextWidth) {
    views::View* box_layout_view =
        content_view_->AddChildView(CreateHorizontalBoxLayoutView());
    box_layout_view->AddChildView(std::move(language_text_label));
    views::FlexLayoutView* buttons_view =
        box_layout_view->AddChildView(CreateHorizontalFlexLayoutView());

    return buttons_view;
  }

  content_view_->AddChildView(std::move(language_text_label));
  return nullptr;
}

void RichAnswersTranslationView::AddReadAndCopyButtons(
    views::View* container_view) {
  CHECK(container_view);

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
      vector_icons::kVolumeUpIcon, ui::kColorSysOnSurface,
      /*icon_size=*/kRichAnswersIconSizeDip);
  container_view->AddChildView(CreateImageButtonView(
      read_closure, read_image_model, ui::kColorSysStateHoverOnSubtle,
      l10n_util::GetStringUTF16(
          IDS_RICH_ANSWERS_VIEW_TRANSLATION_READ_BUTTON_A11Y_NAME_TEXT)));

  // Copy button.
  base::RepeatingClosure copy_closure = base::BindRepeating(
      &RichAnswersTranslationView::OnCopyButtonPressed,
      weak_factory_.GetWeakPtr(), translation_result_.translated_text);
  ui::ImageModel copy_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kContentCopyIcon, ui::kColorSysOnSurface,
      /*icon_size=*/kRichAnswersIconSizeDip);
  container_view->AddChildView(CreateImageButtonView(
      copy_closure, copy_image_model, ui::kColorSysStateHoverOnSubtle,
      l10n_util::GetStringUTF16(
          IDS_RICH_ANSWERS_VIEW_TRANSLATION_COPY_BUTTON_A11Y_NAME_TEXT)));
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

BEGIN_METADATA(RichAnswersTranslationView)
END_METADATA

}  // namespace quick_answers
