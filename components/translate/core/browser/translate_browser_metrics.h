// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_

#include <stddef.h>

#include <string_view>

namespace translate::TranslateBrowserMetrics {

enum class HrefTranslateStatus {
  kAutoTranslated,
  kAutoTranslatedDifferentTargetLanguage,

  // Deprecated, use the below values instead.
  kDeprecatedNotAutoTranslated,

  kUiShownNotAutoTranslated,
  kNoUiShownNotAutoTranslated,

  // Insert new items here. Keep in sync with HrefTranslateStatus in enums.xml
  // when adding values.
  kMaxValue = kNoUiShownNotAutoTranslated
};

enum class TargetLanguageOrigin {
  kRecentTarget,
  kLanguageModel,
  kApplicationUI,
  kAcceptLanguages,
  kDefaultEnglish,
  kChangedByUser,
  kUninitialized,
  kAutoTranslate,
  // Insert new items here. Keep in sync with TranslateTargetLanguageOrigin in
  // enums.xml when adding values.
  kMaxValue = kAutoTranslate
};

enum class MenuTranslationUnavailableReason {
  kTranslateDisabled,
  kNetworkOffline,
  kApiKeysMissing,
  kMIMETypeUnsupported,
  kURLNotTranslatable,
  kTargetLangUnknown,
  kNotAllowedByPolicy,
  kSourceLangUnknown,
  // Insert new items here. Keep in sync with MenuTranslationUnavailableReason
  // in enums.xml when adding values.
  kMaxValue = kSourceLangUnknown
};

// Called when the context (Desktop) menu or app (Mobile) menu is shown and
// manual translation is unavailable to report a reason it is unavailable.
void ReportMenuTranslationUnavailableReason(
    MenuTranslationUnavailableReason reason);

// Called when language detection details are complete.
void ReportLanguageDetectionContentLength(size_t length);

// Called when a request is sent to the translate server to report the source
// language of the translated page. Buckets are labelled with LocaleCodeISO639
// values.
void ReportTranslateSourceLanguage(std::string_view language);

// Called when a request is sent to the translate server to report the target
// language for the translated page. Buckets are labelled with LocaleCodeISO639
// values.
void ReportTranslateTargetLanguage(std::string_view language);

// Called when Chrome Translate is initiated, the navigation is from Google, and
// a href translate target is present.
void ReportTranslateHrefHintStatus(HrefTranslateStatus status);

}  // namespace translate::TranslateBrowserMetrics

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
