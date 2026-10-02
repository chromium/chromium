// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_translate_observer.h"

#include <utility>

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"

ReadAnythingTranslateObserver::ReadAnythingTranslateObserver(
    LanguageCallback on_language_determined)
    : on_language_determined_(std::move(on_language_determined)) {}

ReadAnythingTranslateObserver::~ReadAnythingTranslateObserver() = default;

void ReadAnythingTranslateObserver::Observe(
    content::WebContents& web_contents) {
  ObserveLanguage(ChromeTranslateClient::FromWebContents(&web_contents));
}

void ReadAnythingTranslateObserver::Reset() {
  language_observation_.Reset();
}

void ReadAnythingTranslateObserver::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  on_language_determined_.Run(details.adopted_language);
}

void ReadAnythingTranslateObserver::OnTranslateDriverDestroyed(
    translate::TranslateDriver* driver) {
  language_observation_.Reset();
}

void ReadAnythingTranslateObserver::ObserveLanguage(
    ChromeTranslateClient* translate_client) {
  // Without a translate client there's no language to observe. Stop observing
  // the previous contents so its language isn't reported for these contents.
  if (!translate_client) {
    language_observation_.Reset();
    return;
  }

  // The translate client owns its driver for its whole lifetime.
  translate::TranslateDriver* driver = translate_client->translate_driver();
  const std::string& source_language =
      translate_client->GetLanguageState().source_language();
  // If these web contents are not being observed, then observe them to
  // get a callback when the language is determined. Otherwise,
  // report the language directly.
  if (!language_observation_.IsObservingSource(driver)) {
    language_observation_.Reset();
    language_observation_.Observe(driver);
    // The language may have already been determined before (and then
    // unobserved), so report the language if it's not empty. If the language
    // is outdated, a call to OnLanguageDetermined will report the updated
    // language.
    if (!source_language.empty()) {
      on_language_determined_.Run(source_language);
    }
  } else {
    on_language_determined_.Run(source_language);
  }
}
