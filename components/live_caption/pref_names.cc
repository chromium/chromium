// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/pref_names.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/soda/constants.h"
#include "media/base/media_switches.h"
#endif

namespace prefs {

#if !defined(ANDROID)
// Whether the Live Caption feature is enabled.
const char kLiveCaptionEnabled[] =
    "accessibility.captions.live_caption_enabled";

// The language to use with the Live Caption feature.
const char kLiveCaptionLanguageCode[] =
    "accessibility.captions.live_caption_language";

// The list of origins that should not display an error message when using the
// Media Foundation renderer.
const char kLiveCaptionMediaFoundationRendererErrorSilenced[] =
    "accessibility.captions.live_caption_media_foundation_renderer_error_"
    "silenced";

const std::string GetLiveCaptionLanguageCode(PrefService* profile_prefs) {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage))
    return profile_prefs->GetString(prefs::kLiveCaptionLanguageCode);

  // Default to en-US if the kLiveCaptionMultiLanguage feature isn't enabled.
  return speech::kUsEnglishLocale;
}

bool IsLanguageCodeForLiveCaption(speech::LanguageCode language_code,
                                  PrefService* profile_prefs) {
  return language_code ==
         speech::GetLanguageCode(GetLiveCaptionLanguageCode(profile_prefs));
}

#endif  // !defined(ANDROID)

// String indicating the size of the captions text as a percentage.
const char kAccessibilityCaptionsTextSize[] =
    "accessibility.captions.text_size";

// String indicating the font of the captions text.
const char kAccessibilityCaptionsTextFont[] =
    "accessibility.captions.text_font";

// Comma-separated string indicating the RGB values of the captions text color.
const char kAccessibilityCaptionsTextColor[] =
    "accessibility.captions.text_color";

// Integer indicating the opacity of the captions text from 0 - 100.
const char kAccessibilityCaptionsTextOpacity[] =
    "accessibility.captions.text_opacity";

// Comma-separated string indicating the RGB values of the background color.
const char kAccessibilityCaptionsBackgroundColor[] =
    "accessibility.captions.background_color";

// CSS string indicating the shadow of the captions text.
const char kAccessibilityCaptionsTextShadow[] =
    "accessibility.captions.text_shadow";

// Integer indicating the opacity of the captions text background from 0 - 100.
const char kAccessibilityCaptionsBackgroundOpacity[] =
    "accessibility.captions.background_opacity";

}  // namespace prefs
