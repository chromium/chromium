// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"

#include "base/strings/escape.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/public/browser/tts_utterance.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

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

void QuickAnswersUtteranceEventDelegate::OnTtsEvent(
    content::TtsUtterance* utterance,
    content::TtsEventType event_type,
    int char_index,
    int char_length,
    const std::string& error_message) {
  // For quick answers, the TTS events of interest are START, END, and ERROR.
  switch (event_type) {
    case content::TTS_EVENT_START:
      quick_answers::RecordTtsEngineEvent(
          quick_answers::TtsEngineEvent::TTS_EVENT_START);
      break;
    case content::TTS_EVENT_END:
      quick_answers::RecordTtsEngineEvent(
          quick_answers::TtsEngineEvent::TTS_EVENT_END);
      break;
    case content::TTS_EVENT_ERROR:
      VLOG(1) << __func__ << ": " << error_message;
      quick_answers::RecordTtsEngineEvent(
          quick_answers::TtsEngineEvent::TTS_EVENT_ERROR);
      break;
    case content::TTS_EVENT_WORD:
    case content::TTS_EVENT_SENTENCE:
    case content::TTS_EVENT_MARKER:
    case content::TTS_EVENT_INTERRUPTED:
    case content::TTS_EVENT_CANCELLED:
    case content::TTS_EVENT_PAUSE:
    case content::TTS_EVENT_RESUME:
      // Group the remaining TTS events that aren't of interest together
      // into an unspecified "other" category.
      quick_answers::RecordTtsEngineEvent(
          quick_answers::TtsEngineEvent::TTS_EVENT_OTHER);
      break;
  }

  if (utterance->IsFinished()) {
    delete this;
  }
}

gfx::FontList GetFontList(TypographyToken token) {
  std::vector<std::string> kGoogleSansFontFamily = {kGoogleSansFont,
                                                    kRobotoFont};

  switch (token) {
    case TypographyToken::kCrosBody2:
      return gfx::FontList(kGoogleSansFontFamily, gfx::Font::NORMAL,
                           /*font_size=*/13, gfx::Font::Weight::NORMAL);
    case TypographyToken::kCrosBody2Italic:
      return gfx::FontList(kGoogleSansFontFamily, gfx::Font::ITALIC,
                           /*font_size=*/13, gfx::Font::Weight::NORMAL);
    case TypographyToken::kCrosButton1:
      return gfx::FontList(kGoogleSansFontFamily, gfx::Font::NORMAL,
                           /*font_size=*/14, gfx::Font::Weight::MEDIUM);
    case TypographyToken::kCrosButton2:
      return gfx::FontList(kGoogleSansFontFamily, gfx::Font::NORMAL,
                           /*font_size=*/13, gfx::Font::Weight::MEDIUM);
    case TypographyToken::kCrosDisplay5:
      return gfx::FontList(kGoogleSansFontFamily, gfx::Font::NORMAL,
                           /*font_size=*/24, gfx::Font::Weight::MEDIUM);
    case TypographyToken::kCrosTitle1:
      return gfx::FontList(kGoogleSansFontFamily, gfx::Font::NORMAL,
                           /*font_size=*/16, gfx::Font::Weight::MEDIUM);
  }
}

const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type) {
  switch (result_type) {
    case ResultType::kDefinitionResult:
      return chromeos::kDictionaryIcon;
    case ResultType::kTranslationResult:
      return omnibox::kAnswerTranslationIcon;
    case ResultType::kUnitConversionResult:
      return omnibox::kAnswerCalculatorIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

View* AddHorizontalUiElements(
    View* container,
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements) {
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

std::unique_ptr<views::BoxLayoutView> CreateVerticalBoxLayoutView() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
      .SetBetweenChildSpacing(kContentSingleSpacing)
      .Build();
}

std::unique_ptr<views::BoxLayoutView> CreateHorizontalBoxLayoutView() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetBetweenChildSpacing(kContentSingleSpacing)
      .Build();
}

std::unique_ptr<views::FlexLayoutView> CreateHorizontalFlexLayoutView() {
  std::unique_ptr<views::FlexLayoutView> horizontal_view =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .Build();
  horizontal_view->SetDefault(views::kMarginsKey,
                              kViewHorizontalSpacingMargins);

  return horizontal_view;
}

std::unique_ptr<views::Separator> CreateSeparatorView() {
  return views::Builder<views::Separator>()
      .SetOrientation(views::Separator::Orientation::kHorizontal)
      .SetColorId(ui::kColorSeparator)
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(kContentSingleSpacing, 0, kContentSingleSpacing, 0))
      .Build();
}

std::unique_ptr<views::ImageButton> CreateImageButtonView(
    base::RepeatingClosure closure,
    ui::ImageModel image_model,
    ui::ColorId background_color,
    std::u16string tooltip_text) {
  std::unique_ptr<views::ImageButton> image_button =
      std::make_unique<views::ImageButton>(closure);
  image_button->SetBackground(views::CreateThemedRoundedRectBackground(
      background_color, kRichAnswersIconContainerRadius));
  image_button->SetBorder(views::CreateEmptyBorder(kRichAnswersIconBorderDip));
  image_button->SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                              image_model);
  image_button->SetTooltipText(tooltip_text);

  return image_button;
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

void GenerateTTSAudio(content::BrowserContext* browser_context,
                      const std::string& text,
                      const std::string& locale) {
  auto* tts_controller = content::TtsControllerImpl::GetInstance();
  std::unique_ptr<content::TtsUtterance> tts_utterance =
      content::TtsUtterance::Create(browser_context);

  tts_controller->SetStopSpeakingWhenHidden(false);

  tts_utterance->SetShouldClearQueue(false);
  tts_utterance->SetText(text);
  tts_utterance->SetLang(locale);
  // TtsController will use the default TTS engine if the Google TTS engine
  // is not available.
  tts_utterance->SetEngineId(kGoogleTtsEngineId);
  tts_utterance->SetEventDelegate(new QuickAnswersUtteranceEventDelegate());

  tts_controller->SpeakOrEnqueue(std::move(tts_utterance));
}

}  // namespace quick_answers
