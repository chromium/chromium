// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_translate_observer.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_features.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

// Every tab has a ChromeTranslateClient (see TabHelpers::AttachTabHelpers()),
// so this can only be used for the tab's contents.
ChromeTranslateClient& GetTabTranslateClient(
    content::WebContents& tab_contents) {
  return CHECK_DEREF(ChromeTranslateClient::FromWebContents(&tab_contents));
}

}  // namespace

ReadAnythingTranslateObserver::ReadAnythingTranslateObserver(
    LanguageCallback on_language_determined,
    TranslationStateCallback on_translation_state_changed)
    : on_language_determined_(std::move(on_language_determined)),
      on_translation_state_changed_(std::move(on_translation_state_changed)) {}

ReadAnythingTranslateObserver::~ReadAnythingTranslateObserver() = default;

void ReadAnythingTranslateObserver::Observe(
    content::WebContents& tab_contents,
    content::WebContents& web_contents) {
  ChromeTranslateClient& tab_translate_client =
      GetTabTranslateClient(tab_contents);
  ChromeTranslateClient* translate_client = &tab_translate_client;
#if BUILDFLAG(ENABLE_PDF)
  // TODO(crbug.com/340272378): When removing this feature flag, remove this
  // special case and observe the language on the tab's contents too.
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    // In the non-OOPIF PDF viewer, `web_contents` may be the PDF's inner
    // contents, which, unlike the tab's contents, may not have a translate
    // client.
    translate_client = ChromeTranslateClient::FromWebContents(&web_contents);
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  ObserveLanguage(translate_client);
  ObserveTranslationState(tab_translate_client);
}

void ReadAnythingTranslateObserver::Reset() {
  language_observation_.Reset();
  translation_observation_.Reset();
}

void ReadAnythingTranslateObserver::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  on_language_determined_.Run(details.adopted_language);
}

void ReadAnythingTranslateObserver::OnTranslateDriverDestroyed(
    translate::TranslateDriver* driver) {
  // This is called from ~TranslateDriver(), after ~ContentTranslateDriver() has
  // run, so don't reset `translation_observation_` here: that would call into
  // the partially destroyed ContentTranslateDriver. The tab's driver, the only
  // one observed for translation state, is instead unobserved through Reset()
  // before the tab's contents is destroyed.
  language_observation_.Reset();
}

void ReadAnythingTranslateObserver::OnIsPageTranslatedChanged(
    content::WebContents* source) {
  // Translation state is only observed when the feature is enabled (see
  // ObserveTranslationState()).
  CHECK(base::FeatureList::IsEnabled(translate::kEnableTranslatePdf));
  // Only the tab's driver is observed for translation state, and a driver
  // always has its contents while it's alive.
  ReportTranslationState(GetTabTranslateClient(CHECK_DEREF(source)));
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

void ReadAnythingTranslateObserver::ObserveTranslationState(
    ChromeTranslateClient& tab_translate_client) {
  if (!base::FeatureList::IsEnabled(translate::kEnableTranslatePdf)) {
    return;
  }

  // The translate client owns its driver for its whole lifetime.
  translate::ContentTranslateDriver* driver =
      tab_translate_client.translate_driver();
  if (translation_observation_.IsObservingSource(driver)) {
    return;
  }

  translation_observation_.Reset();
  translation_observation_.Observe(driver);
  // The translation state may have changed while it wasn't observed, so report
  // the current state.
  ReportTranslationState(tab_translate_client);
}

void ReadAnythingTranslateObserver::ReportTranslationState(
    ChromeTranslateClient& tab_translate_client) {
  CHECK(base::FeatureList::IsEnabled(translate::kEnableTranslatePdf));
  on_translation_state_changed_.Run(
      tab_translate_client.GetLanguageState().IsPageTranslated());
}
