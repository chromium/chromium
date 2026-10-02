// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TRANSLATE_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TRANSLATE_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/translate/core/browser/translate_driver.h"

class ChromeTranslateClient;

namespace content {
class WebContents;
}  // namespace content

namespace translate {
struct LanguageDetectionDetails;
}  // namespace translate

// Observes the translate state that reading mode needs from the page it
// displays: the page's language. Owned by ReadAnythingUntrustedPageHandler,
// which is notified through the callback passed to the constructor.
//
// This keeps all of reading mode's observation of translate::TranslateDriver in
// one place.
class ReadAnythingTranslateObserver
    : public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  // Called with the page's language code once it's known. The code may be
  // empty or translate's "unknown" language code.
  using LanguageCallback = base::RepeatingCallback<void(const std::string&)>;

  explicit ReadAnythingTranslateObserver(
      LanguageCallback on_language_determined);
  ReadAnythingTranslateObserver(const ReadAnythingTranslateObserver&) = delete;
  ReadAnythingTranslateObserver& operator=(
      const ReadAnythingTranslateObserver&) = delete;
  ~ReadAnythingTranslateObserver() override;

  // Starts observing the language of `web_contents`, the contents with the
  // content that reading mode displays, if not already observing it, and
  // reports its current language.
  void Observe(content::WebContents& web_contents);

  // Stops observing.
  void Reset();

  // translate::TranslateDriver::LanguageDetectionObserver:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;

 private:
  // Observes the source language through `translate_client`, or stops
  // observing the source language if it's null.
  void ObserveLanguage(ChromeTranslateClient* translate_client);

  LanguageCallback on_language_determined_;

  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      language_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TRANSLATE_OBSERVER_H_
