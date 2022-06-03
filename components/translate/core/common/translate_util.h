// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_

#include "base/feature_list.h"
#include "url/gurl.h"

namespace translate {

// Controls whether translation applies to sub frames as well as the
// main frame.
extern const base::Feature kTranslateSubFrames;

// Controls whether the TFLite-based language detection is enabled.
extern const base::Feature kTFLiteLanguageDetectionEnabled;

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

// Return the threshold used to determine if TFLite language detection model's
// prediction is reliable.
float GetTFLiteLanguageDetectionThreshold();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
