// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/tts_utterance.h"
#include "ui/gfx/geometry/insets.h"

namespace quick_answers {

// Google icon.
inline constexpr int kGoogleIconSizeDip = 16;

// Other icons
inline constexpr int kIconSizeDip = 16;

// View constants.
inline constexpr gfx::Insets kMainViewInsets = gfx::Insets::TLBR(12, 8, 12, 16);

// TTS constants.
inline constexpr char kGoogleTtsEngineId[] = "com.google.android.tts";

// The lifetime of instances of this class is manually bound to the lifetime of
// the associated TtsUtterance. See OnTtsEvent.
class QuickAnswersUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  QuickAnswersUtteranceEventDelegate() = default;
  ~QuickAnswersUtteranceEventDelegate() override = default;

  // UtteranceEventDelegate methods:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override;
};

// Return the icon that corresponds to the Quick Answers result type.
const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type);

// Return the GURL that will link to the google search result for the
// query text.
GURL GetDetailsUrlForQuery(const std::string& query);

void GenerateTTSAudio(content::BrowserContext* browser_context,
                      const std::string& text,
                      const std::string& locale);

const gfx::Insets GetMainViewInsets();

const gfx::Insets GetButtonsViewInsets();

int GetButtonsViewOcclusion();

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
