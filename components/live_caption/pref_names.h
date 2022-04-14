// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_
#define COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_

#include <string>

#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/soda/constants.h"
#endif

class PrefService;

namespace prefs {

// Live Caption is not available on Android, so exclude these unneeded
// kLiveCaption*  prefs.
#if !defined(ANDROID)
extern const char kLiveCaptionEnabled[];
extern const char kLiveCaptionLanguageCode[];
extern const char kLiveCaptionMediaFoundationRendererErrorSilenced[];

const std::string GetLiveCaptionLanguageCode(PrefService* profile_prefs);
bool IsLanguageCodeForLiveCaption(speech::LanguageCode language_code,
                                  PrefService* profile_prefs);

#endif  // !defined(ANDROID)

// These kAccessibilityCaptions* caption style prefs are used on Android
// (though their primary use is for Live Caption, which is why they are housed
// within this live_caption component instead of somewhere more generic).
extern const char kAccessibilityCaptionsTextSize[];
extern const char kAccessibilityCaptionsTextFont[];
extern const char kAccessibilityCaptionsTextColor[];
extern const char kAccessibilityCaptionsTextOpacity[];
extern const char kAccessibilityCaptionsBackgroundColor[];
extern const char kAccessibilityCaptionsTextShadow[];
extern const char kAccessibilityCaptionsBackgroundOpacity[];

}  // namespace prefs

#endif  // COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_
