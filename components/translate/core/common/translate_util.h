// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace translate {

// Controls whether the TFLite-based language detection is enabled.
BASE_DECLARE_FEATURE(kTFLiteLanguageDetectionEnabled);

// Isolated world sets following security-origin by default.
extern const char kSecurityOrigin[];

// Gets Security origin with which Translate runs. This is used both for
// language checks and to obtain the list of available languages.
GURL GetTranslateSecurityOrigin();

// Return whether TFLite-based language detection is enabled.
bool IsTFLiteLanguageDetectionEnabled();

// Return the threshold used to determine if TFLite language detection model's
// prediction is reliable.
float GetTFLiteLanguageDetectionThreshold();

// Feature flag used to control the auto-always and auto-never snackbar
// parameters (i.e. threshold and maximum-number-of).
BASE_DECLARE_FEATURE(kTranslateAutoSnackbars);

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
