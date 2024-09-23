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
#include "ui/color/color_id.h"
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

constexpr char kBulletSymbol[] = " \u2022 ";

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

  // Set up the subcontent view that contains all the definition info other than
  // the word, phonetics info, and word class.
  SetUpSubContentView();

  AddDefinition(subcontent_view_, definition_result_.sense,
                kSubContentTextWidth);

  MaybeAddSampleSentence(subcontent_view_, definition_result_.sense,
                         kSubContentTextWidth);

  MaybeAddSynonyms(subcontent_view_, definition_result_.sense,
                   kSubContentTextWidth);

  MaybeAddAdditionalDefinitions();
}

void RichAnswersDefinitionView::AddHeaderViews() {
  // This box layout will have the view flex values as:
  // - header_view (flex=1): resize (either shrink or expand as necessary)
  // - settings_button_view (flex=0): no resize
  views::BoxLayoutView* box_layout_view =
      content_view_->AddChildView(CreateHorizontalBoxLayoutView());
  box_layout_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  views::FlexLayoutView* header_view =
      box_layout_view->AddChildView(CreateHorizontalFlexLayoutView());

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
    int header_space_available =
        kDefinitionHeaderTextWidth - word_label->GetPreferredSize().width();
    int phonetics_label_width =
        phonetics_label->GetPreferredSize().width() + kContentSingleSpacing;
    if (phonetics_label_width <= header_space_available) {
      header_view->AddChildView(std::move(phonetics_label));
    } else {
      content_view_->AddChildView(std::move(phonetics_label));
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
                                     ui::kColorSysOnSurface,
                                     kRichAnswersIconSizeDip);
  views::ImageButton* button_view =
      container_view->AddChildView(CreateImageButtonView(
          phonetics_audio_button_closure,
          phonetics_audio_button_closure_image_model,
          ui::kColorSysStateHoverOnSubtle,
          l10n_util::GetStringUTF16(
              IDS_RICH_ANSWERS_VIEW_PHONETICS_BUTTON_A11Y_NAME_TEXT)));
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
  content_view_->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      definition_result_.word_class,
      GetFontList(TypographyToken::kCrosBody2Italic), kContentTextWidth,
      /*is_multi_line=*/false, ui::kColorSysSecondary));
}

void RichAnswersDefinitionView::SetUpSubContentView() {
  subcontent_view_ = content_view_->AddChildView(CreateVerticalBoxLayoutView());
  subcontent_view_->SetMinimumCrossAxisSize(kSubContentTextWidth);
  subcontent_view_->SetInsideBorderInsets(kSubContentViewInsets);
}

void RichAnswersDefinitionView::AddDefinition(views::View* container_view,
                                              const Sense& sense,
                                              int label_width) {
  container_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      sense.definition, GetFontList(TypographyToken::kCrosBody2), label_width,
      /*is_multi_line=*/true, ui::kColorSysOnSurface));
}

void RichAnswersDefinitionView::MaybeAddSampleSentence(
    views::View* container_view,
    const Sense& sense,
    int label_width) {
  if (!sense.sample_sentence) {
    return;
  }

  container_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      "\"" + sense.sample_sentence.value() + "\"",
      GetFontList(TypographyToken::kCrosBody2), label_width,
      /*is_multi_line=*/true, ui::kColorSysSecondary));
}

void RichAnswersDefinitionView::MaybeAddSynonyms(views::View* container_view,
                                                 const Sense& sense,
                                                 int label_width) {
  if (!sense.synonyms_list) {
    return;
  }

  // This box layout will have the view flex values as:
  // - similar_label (flex=0): no resize
  // - synonyms_label (flex=1): resize (either shrink or expand as necessary)
  views::BoxLayoutView* box_layout_view =
      container_view->AddChildView(CreateHorizontalBoxLayoutView());
  box_layout_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  QuickAnswersTextLabel* similar_label =
      box_layout_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          l10n_util::GetStringUTF8(
              IDS_RICH_ANSWERS_VIEW_DEFINITION_SYNONYMS_LABEL_TEXT),
          GetFontList(TypographyToken::kCrosBody2), label_width,
          /*is_multi_line=*/true, ui::kColorCrosSysPositive));
  std::string synonyms_text =
      base::JoinString(sense.synonyms_list.value(), ", ");
  int synonyms_label_width = label_width -
                             similar_label->GetPreferredSize().width() -
                             kContentSingleSpacing;
  QuickAnswersTextLabel* synonyms_label =
      box_layout_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          synonyms_text, GetFontList(TypographyToken::kCrosBody2),
          synonyms_label_width,
          /*is_multi_line=*/true, ui::kColorSysSecondary));

  box_layout_view->SetFlexForView(similar_label, /*flex=*/0);
  box_layout_view->SetFlexForView(synonyms_label, /*flex=*/1);
}

void RichAnswersDefinitionView::MaybeAddAdditionalDefinitions() {
  if (!definition_result_.subsenses_list) {
    return;
  }

  for (const Sense& subsense : *definition_result_.subsenses_list) {
    AddSubsense(subsense);
  }
}

void RichAnswersDefinitionView::AddSubsense(const Sense& subsense) {
  // This box layout will have the view flex values as:
  // - bullet_label (flex=0): no resize
  // - subsense_view (flex=1): resize (either shrink or expand as necessary)
  views::BoxLayoutView* box_layout_view =
      subcontent_view_->AddChildView(CreateHorizontalBoxLayoutView());
  box_layout_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  QuickAnswersTextLabel* bullet_label =
      box_layout_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          kBulletSymbol, GetFontList(TypographyToken::kCrosBody2),
          kSubContentTextWidth,
          /*is_multi_line=*/false, ui::kColorSysOnSurface));

  views::BoxLayoutView* subsense_view =
      box_layout_view->AddChildView(CreateVerticalBoxLayoutView());
  int subsense_labels_width = kSubContentTextWidth -
                              bullet_label->GetPreferredSize().width() -
                              kContentSingleSpacing;
  subsense_view->SetMinimumCrossAxisSize(subsense_labels_width);
  AddDefinition(subsense_view, subsense, subsense_labels_width);
  MaybeAddSampleSentence(subsense_view, subsense, subsense_labels_width);
  MaybeAddSynonyms(subsense_view, subsense, subsense_labels_width);

  box_layout_view->SetFlexForView(bullet_label, /*flex=*/0);
  box_layout_view->SetFlexForView(subsense_view, /*flex=*/1);
}

BEGIN_METADATA(RichAnswersDefinitionView)
END_METADATA

}  // namespace quick_answers
