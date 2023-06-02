// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_

#include <stddef.h>

#include "base/strings/string_piece.h"

namespace translate {

namespace TranslateBrowserMetrics {

// When Chrome Translate is ready to translate a page, one of following reasons
// decides the next browser action.
// Note: Don't insert items. It will change the reporting UMA value and break
// the UMA dashboard page. Instead, append it at the end of enum as suggested
// below. Added items also need to be added to TranslateInitiationStatus in
// enums.xml.
enum InitiationStatusType {
  INITIATION_STATUS_DISABLED_BY_PREFS,
  INITIATION_STATUS_DISABLED_BY_SWITCH,
  INITIATION_STATUS_DISABLED_BY_CONFIG,
  INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED,
  INITIATION_STATUS_URL_IS_NOT_SUPPORTED,
  INITIATION_STATUS_SIMILAR_LANGUAGES,
  INITIATION_STATUS_ACCEPT_LANGUAGES,
  INITIATION_STATUS_AUTO_BY_CONFIG,
  INITIATION_STATUS_AUTO_BY_LINK,
  INITIATION_STATUS_SHOW_INFOBAR,
  INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED,
  INITIATION_STATUS_DISABLED_BY_KEY,
  INITIATION_STATUS_LANGUAGE_IN_ULP,
  INITIATION_STATUS_ABORTED_BY_RANKER,

  // Deprecated, never used in practice
  DEPRECATED_INITIATION_STATUS_ABORTED_BY_TOO_OFTEN_DENIED,

  INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE,
  INITIATION_STATUS_CREATE_INFOBAR,
  INITIATION_STATUS_SHOW_ICON,
  INITIATION_STATUS_SUPPRESS_INFOBAR,
  INITIATION_STATUS_SHOW_UI_PREDEFINED_TARGET_LANGUAGE,
  INITIATION_STATUS_NO_NETWORK,
  INITIATION_STATUS_DOESNT_NEED_TRANSLATION,
  INITIATION_STATUS_IDENTICAL_LANGUAGE_USE_SOURCE_LANGUAGE_UNKNOWN,

  // Deprecated since M110 with the removal of the autofill_assistant component.
  DEPRECATED_STATUS_DISABLED_BY_AUTOFILL_ASSISTANT,

  INITIATION_STATUS_AUTO_BY_PREDEFINED_TARGET_LANGUAGE,
  // Insert new items here.
  INITIATION_STATUS_MAX,
};

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

// Called when Chrome Translate is initiated to report a reason of the next
// browser action.
void ReportInitiationStatus(InitiationStatusType type);

// Called when the context (Desktop) menu or app (Mobile) menu is shown and
// manual translation is unavailable to report a reason it is unavailable.
void ReportMenuTranslationUnavailableReason(
    MenuTranslationUnavailableReason reason);

// Called when language detection details are complete.
void ReportLanguageDetectionContentLength(size_t length);

void ReportUnsupportedLanguageAtInitiation(base::StringPiece language);

// Called when a request is sent to the translate server to report the source
// language of the translated page. Buckets are labelled with LocaleCodeISO639
// values.
void ReportTranslateSourceLanguage(base::StringPiece language);

// Called when a request is sent to the translate server to report the target
// language for the translated page. Buckets are labelled with LocaleCodeISO639
// values.
void ReportTranslateTargetLanguage(base::StringPiece language);

// Called when Chrome Translate is initiated, the navigation is from Google, and
// a href translate target is present.
void ReportTranslateHrefHintStatus(HrefTranslateStatus status);

}  // namespace TranslateBrowserMetrics

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
