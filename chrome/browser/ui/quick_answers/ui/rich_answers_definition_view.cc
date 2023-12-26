// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_definition_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {

// The space available to show text in the definition header view.
// The width of the phonetics audio button needs to be subtracted
// from the total header width.
constexpr int kDefinitionHeaderTextWidth =
    quick_answers::kContentHeaderWidth -
    (quick_answers::kContentSingleSpacing +
     quick_answers::kRichAnswersIconContainerRadius);

}  // namespace

namespace quick_answers {

// RichAnswersDefinitionView
// -----------------------------------------------------------

RichAnswersDefinitionView::RichAnswersDefinitionView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const DefinitionResult& definition_result)
    : RichAnswersView(anchor_view_bounds,
                      controller,
                      ResultType::kDefinitionResult),
      definition_result_(definition_result) {
  InitLayout();

  // TODO (b/274184670): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersDefinitionView::~RichAnswersDefinitionView() = default;

void RichAnswersDefinitionView::InitLayout() {
  content_view_ = GetContentView();

  // Word and phonetics.
  AddHeaderViews();

  AddWordClass();

  AddDefinition();

  MaybeAddSampleSentence();

  MaybeAddSynonyms();
}

void RichAnswersDefinitionView::AddHeaderViews() {
  views::View* container_view = AddFillLayoutChildView(content_view_);
  // This box layout will have the view flex values as:
  // - header_view (flex=1): resize (either shrink or expand as necessary)
  // - settings_button_view (flex=0): no resize
  views::BoxLayoutView* box_layout_view =
      container_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  views::FlexLayoutView* header_view =
      box_layout_view->AddChildView(CreateHorizontalLayoutView());

  QuickAnswersTextLabel* word_label =
      header_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          definition_result_.word, GetFontList(TypographyToken::kCrosTitle1),
          kDefinitionHeaderTextWidth,
          /*is_multi_line=*/false));

  // The phonetics text label is an optional child view in the header.
  // Check that the phonetics text is not empty.
  if (!definition_result_.phonetics_info.text.empty()) {
    std::unique_ptr<QuickAnswersTextLabel> phonetics_label =
        QuickAnswersTextLabel::CreateLabelWithStyle(
            "/" + definition_result_.phonetics_info.text + "/",
            GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
            /*is_multi_line=*/false);

    // Display the phonetics label in the header view if it fits, otherwise show
    // it in a subheader view below.
    int header_space_available = kDefinitionHeaderTextWidth -
                                 word_label->CalculatePreferredSize().width();
    int phonetics_label_width =
        phonetics_label->CalculatePreferredSize().width() +
        kContentSingleSpacing;
    if (phonetics_label_width <= header_space_available) {
      header_view->AddChildView(std::move(phonetics_label));
    } else {
      AddFillLayoutChildView(content_view_, std::move(phonetics_label));
    }
  }

  AddPhoneticsAudioButtonTo(header_view);
  views::View* settings_button_view = AddSettingsButtonTo(box_layout_view);

  box_layout_view->SetFlexForView(header_view, /*flex=*/1);
  box_layout_view->SetFlexForView(settings_button_view, /*flex=*/0);
}

void RichAnswersDefinitionView::AddPhoneticsAudioButtonTo(
    views::View* container_view) {
  // Setup an invisible web view to play TTS audio.
  tts_audio_web_view_ = container_view->AddChildView(
      std::make_unique<views::WebView>(ProfileManager::GetActiveUserProfile()));
  tts_audio_web_view_->SetVisible(false);

  base::RepeatingClosure phonetics_audio_button_closure = base::BindRepeating(
      &RichAnswersDefinitionView::OnPhoneticsAudioButtonPressed,
      weak_factory_.GetWeakPtr());
  ui::ImageModel phonetics_audio_button_closure_image_model =
      ui::ImageModel::FromVectorIcon(vector_icons::kVolumeUpIcon,
                                     cros_tokens::kCrosSysOnSurface,
                                     kRichAnswersIconSizeDip);
  views::ImageButton* button_view =
      container_view->AddChildView(CreateImageButtonView(
          phonetics_audio_button_closure,
          phonetics_audio_button_closure_image_model,
          cros_tokens::kCrosSysHoverOnSubtle,
          l10n_util::GetStringUTF16(
              IDS_QUICK_ANSWERS_PHONETICS_BUTTON_TOOLTIP_TEXT)));
  button_view->SetMinimumImageSize(
      gfx::Size(kRichAnswersIconSizeDip, kRichAnswersIconSizeDip));
}

void RichAnswersDefinitionView::OnPhoneticsAudioButtonPressed() {
  PhoneticsInfo phonetics_info = definition_result_.phonetics_info;
  // Use the phonetics audio URL if provided.
  if (!phonetics_info.phonetics_audio.is_empty()) {
    tts_audio_web_view_->LoadInitialURL(phonetics_info.phonetics_audio);
    return;
  }

  GenerateTTSAudio(tts_audio_web_view_->GetBrowserContext(),
                   phonetics_info.query_text, phonetics_info.locale);
}

void RichAnswersDefinitionView::AddWordClass() {
  AddFillLayoutChildView(
      content_view_,
      QuickAnswersTextLabel::CreateLabelWithStyle(
          definition_result_.word_class,
          GetFontList(TypographyToken::kCrosBody2Italic), kContentTextWidth,
          /*is_multi_line=*/false, cros_tokens::kCrosSysSecondary));
}

void RichAnswersDefinitionView::AddDefinition() {
  AddFillLayoutChildView(
      content_view_,
      QuickAnswersTextLabel::CreateLabelWithStyle(
          definition_result_.sense.definition,
          GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
          /*is_multi_line=*/true, cros_tokens::kCrosSysOnSurface));
}

void RichAnswersDefinitionView::MaybeAddSampleSentence() {
  std::optional<std::string> sample_sentence =
      definition_result_.sense.sample_sentence;
  if (!sample_sentence.has_value()) {
    return;
  }

  AddFillLayoutChildView(
      content_view_,
      QuickAnswersTextLabel::CreateLabelWithStyle(
          "\"" + sample_sentence.value() + "\"",
          GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
          /*is_multi_line=*/true, cros_tokens::kCrosSysSecondary));
}

void RichAnswersDefinitionView::MaybeAddSynonyms() {
  std::optional<std::vector<std::string>> synonyms_list =
      definition_result_.sense.synonyms_list;
  if (!synonyms_list.has_value()) {
    return;
  }

  views::View* container_view = AddFillLayoutChildView(content_view_);
  // This box layout will have the view flex values as:
  // - similar_label (flex=0): no resize
  // - synonyms_label (flex=1): resize (either shrink or expand as necessary)
  views::BoxLayoutView* labels_view =
      container_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  labels_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  labels_view->SetBetweenChildSpacing(kContentSingleSpacing);

  QuickAnswersTextLabel* similar_label =
      labels_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          l10n_util::GetStringUTF8(
              IDS_QUICK_ANSWERS_DEFINITION_SYNONYMS_LABEL_TEXT),
          GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
          /*is_multi_line=*/true, cros_tokens::kHighlightColorGreen));
  std::string synonyms_text = base::JoinString(synonyms_list.value(), ", ");
  QuickAnswersTextLabel* synonyms_label =
      labels_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          synonyms_text, GetFontList(TypographyToken::kCrosBody2),
          kContentTextWidth - similar_label->CalculatePreferredSize().width(),
          /*is_multi_line=*/true, cros_tokens::kCrosSysSecondary));

  labels_view->SetFlexForView(similar_label, /*flex=*/0);
  labels_view->SetFlexForView(synonyms_label, /*flex=*/1);
}

BEGIN_METADATA(RichAnswersDefinitionView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
