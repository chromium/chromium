// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TRANSLATE_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TRANSLATE_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_driver.h"

class ChromeTranslateClient;

namespace content {
class WebContents;
}  // namespace content

namespace translate {
struct LanguageDetectionDetails;
}  // namespace translate

// Observes the translate state that reading mode needs from the page it
// displays: the page's language, and whether the page is translated. Owned by
// ReadAnythingUntrustedPageHandler, which is notified through the callbacks
// passed to the constructor.
//
// This keeps all of reading mode's observation of translate::TranslateDriver in
// one place, since the language and the translation state are reported by two
// different observer interfaces of the same driver.
class ReadAnythingTranslateObserver
    : public translate::TranslateDriver::LanguageDetectionObserver,
      public translate::ContentTranslateDriver::TranslationObserver {
 public:
  // Called with the page's language code once it's known. The code may be
  // empty or translate's "unknown" language code.
  using LanguageCallback = base::RepeatingCallback<void(const std::string&)>;
  // Called with whether the page is translated.
  using TranslationStateCallback = base::RepeatingCallback<void(bool)>;

  ReadAnythingTranslateObserver(
      LanguageCallback on_language_determined,
      TranslationStateCallback on_translation_state_changed);
  ReadAnythingTranslateObserver(const ReadAnythingTranslateObserver&) = delete;
  ReadAnythingTranslateObserver& operator=(
      const ReadAnythingTranslateObserver&) = delete;
  ~ReadAnythingTranslateObserver() override;

  // Starts observing the given contents, if not already observing them, and
  // reports their current state:
  // - The language is observed on `web_contents`, the contents with the
  //   content that reading mode displays. This is only different from
  //   `tab_contents` for PDFs in the non-OOPIF PDF viewer, where it's the PDF's
  //   inner contents; with the OOPIF PDF viewer, the language is always
  //   observed on `tab_contents`.
  // - The translation state is observed on `tab_contents`, since translation
  //   of the content displayed in reading mode, including PDFs, is driven by
  //   the tab's ContentTranslateDriver. Only observed if
  //   translate::kEnableTranslatePdf is enabled.
  void Observe(content::WebContents& tab_contents,
               content::WebContents& web_contents);

  // Stops observing. Must be called before the tab's contents is destroyed or
  // replaced (e.g. from WebContentsObserver::WebContentsDestroyed()):
  // ContentTranslateDriver doesn't notify its TranslationObservers when it's
  // destroyed, so the translation observation can't clean itself up.
  void Reset();

  // translate::TranslateDriver::LanguageDetectionObserver:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;

  // translate::ContentTranslateDriver::TranslationObserver:
  void OnIsPageTranslatedChanged(content::WebContents* source) override;

 private:
  // Observes the source language through `translate_client`, or stops
  // observing the source language if it's null.
  void ObserveLanguage(ChromeTranslateClient* translate_client);
  void ObserveTranslationState(ChromeTranslateClient& tab_translate_client);
  void ReportTranslationState(ChromeTranslateClient& tab_translate_client);

  LanguageCallback on_language_determined_;
  TranslationStateCallback on_translation_state_changed_;

  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      language_observation_{this};
  base::ScopedObservation<
      translate::ContentTranslateDriver,
      translate::ContentTranslateDriver::TranslationObserver>
      translation_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TRANSLATE_OBSERVER_H_
