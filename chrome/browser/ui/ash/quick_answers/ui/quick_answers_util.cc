// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/quick_answers_util.h"

#include "base/i18n/legacy_language_tag_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/public/browser/tts_utterance.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// Google search link.
constexpr char kGoogleSearchUrlPrefix[] = "https://www.google.com/search?q=";
constexpr char kGoogleTranslateUrlTemplate[] =
    "https://translate.google.com/?sl=auto&tl=%s&text=%s&op=translate";
constexpr char kTranslationQueryPrefix[] = "Translate:";

}  // namespace

namespace quick_answers {

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
}

const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type) {
  switch (result_type) {
    case ResultType::kDefinitionResult:
      return chromeos::kDictionaryIcon;
    case ResultType::kTranslationResult:
      return features::IsRoundedIconsEnabled()
                 ? omnibox::kTranslateIcon
                 : omnibox::kAnswerTranslationOldIcon;
    case ResultType::kUnitConversionResult:
      return features::IsRoundedIconsEnabled()
                 ? omnibox::kEqualIcon
                 : omnibox::kAnswerCalculatorOldIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

GURL GetDetailsUrlForQuery(const std::string& query) {
  // TODO(b/240619915): Refactor so that we can access the request metadata
  // instead of just the query itself.
  auto remainder = base::RemovePrefix(query, kTranslationQueryPrefix);
  if (!remainder) {
    return GURL(kGoogleSearchUrlPrefix +
                base::EscapeUrlEncodedData(query, /*use_plus=*/true));
  }
  auto query_text = base::EscapeUrlEncodedData(*remainder, /*use_plus=*/true);
  auto device_language = base::i18n::GetLanguageSubtagUsingLanguageTag(
      QuickAnswersState::Get()->application_locale());
  auto translate_url = base::StringPrintf(kGoogleTranslateUrlTemplate,
                                          device_language, query_text);
  return GURL(translate_url);
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
  tts_utterance->SetEventDelegate(
      std::make_unique<QuickAnswersUtteranceEventDelegate>());

  tts_controller->SpeakOrEnqueue(std::move(tts_utterance));
}

const gfx::Insets GetMainViewInsets() {
  return gfx::Insets::TLBR(12, 16, 16, 16);
}

const gfx::Insets GetButtonsViewInsets() {
  // Buttons view is rendered as a layer on top of main view.
  return GetMainViewInsets();
}

// TODO: crbug.com/340629098 - A temporary solution until buttons view is merged
// into headers. See another comment for buttons view in
// `QuickAnswersView::QuickAnswersView` about details.
int GetButtonsViewOcclusion() {
  gfx::Insets insets_icon_button =
      views::LayoutProvider::Get()->GetInsetsMetric(
          views::InsetsMetric::INSETS_ICON_BUTTON);
  return insets_icon_button.left() + kGoogleIconSizeDip +
         insets_icon_button.right() + GetButtonsViewInsets().right();
}

}  // namespace quick_answers
