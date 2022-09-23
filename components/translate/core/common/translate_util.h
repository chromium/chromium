// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace translate {

// Controls whether translation applies to sub frames as well as the
// main frame.
extern const base::Feature kTranslateSubFrames;

// Controls whether the TFLite-based language detection is enabled.
extern const base::Feature kTFLiteLanguageDetectionEnabled;

// Controls whether the TFLite-based language detection is computed, but ignored
// and the CLD3 version is used instead.
extern const base::Feature kTFLiteLanguageDetectionIgnoreEnabled;

// Controls whether the Partial Translate function is available.
extern const base::Feature kDesktopPartialTranslate;
// The maximum number of characters allowed for a text selection in Partial
// Translate. Longer selections will be truncated down to the first valid word
// break respecting the threshold.
extern const base::FeatureParam<int>
    kDesktopPartialTranslateTextSelectionMaxCharacters;
// The number of milliseconds to wait before showing the Partial Translate
// bubble, even if no response has been received. In this case, a waiting view
// is shown.
extern const base::FeatureParam<int> kDesktopPartialTranslateBubbleShowDelayMs;

#if !BUILDFLAG(IS_WIN)
// Controls whether mmap is used to load the language detection model.
extern const base::Feature kMmapLanguageDetectionModel;
#endif

// Isolated world sets following security-origin by default.
extern const char kSecurityOrigin[];

// Gets Security origin with which Translate runs. This is used both for
// language checks and to obtain the list of available languages.
GURL GetTranslateSecurityOrigin();

// Return whether sub frame translation is enabled.
bool IsSubFrameTranslationEnabled();

// Return whether sub frame language detection is enabled.
bool IsSubFrameLanguageDetectionEnabled();

// Return whether TFLite-based language detection is enabled.
bool IsTFLiteLanguageDetectionEnabled();

// Return whether TFLite-based language detection is enabled, but the result is
// ignored.
bool IsTFLiteLanguageDetectionIgnoreEnabled();

// Return the threshold used to determine if TFLite language detection model's
// prediction is reliable.
float GetTFLiteLanguageDetectionThreshold();

// Feature flag used to control the auto-always and auto-never snackbar
// parameters (i.e. threshold and maximum-number-of).
extern const base::Feature kTranslateAutoSnackbars;

// The number of times the user should consecutively translate for "Always
// Translate" to automatically trigger.
int GetAutoAlwaysThreshold();

// The number of times the user should consecutively dismiss the translate UI
// for "Never Translate" to automatically trigger.
int GetAutoNeverThreshold();

// The maximum number of times "Always Translate" is automatically triggered.
int GetMaximumNumberOfAutoAlways();

// The maximum number of times "Never Translate" is automatically triggered.
int GetMaximumNumberOfAutoNever();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
