// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/translate_waiter.h"

#include "base/run_loop.h"

namespace translate {

TranslateWaiter::TranslateWaiter(ContentTranslateDriver* translate_driver,
                                 WaitEvent wait_event)
    : wait_event_(wait_event),
      // Ensure recursive tasks on the message loop so we get translate script
      // fetch events. This is required for macOS.
      run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {
  scoped_translation_observation_.Observe(translate_driver);
  scoped_language_detection_observation_.Observe(translate_driver);
}

TranslateWaiter::~TranslateWaiter() = default;

void TranslateWaiter::Wait() {
  run_loop_.Run();
}

// TranslateDriver::LanguageDetectionObserver:
void TranslateWaiter::OnLanguageDetermined(
    const LanguageDetectionDetails& details) {
  if (wait_event_ == WaitEvent::kLanguageDetermined)
    run_loop_.Quit();
}

// ContentTranslateDriver::TranslationObserver:
void TranslateWaiter::OnPageTranslated(const std::string& source_lang,
                                       const std::string& translated_lang,
                                       TranslateErrors error_type) {
  if (wait_event_ == WaitEvent::kPageTranslated)
    run_loop_.Quit();
}

void TranslateWaiter::OnIsPageTranslatedChanged(content::WebContents* source) {
  if (wait_event_ == WaitEvent::kIsPageTranslatedChanged)
    run_loop_.Quit();
}

}  // namespace translate
