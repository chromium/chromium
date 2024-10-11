// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_
#define COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/soda/constants.h"
#endif

class PrefService;

namespace prefs {

// Live Caption is not available on Android, so exclude these unneeded
// kLiveCaption*  prefs.
#if !defined(ANDROID)
// Whether the Live Caption bubble is expanded.
inline constexpr char kLiveCaptionBubbleExpanded[] =
    "accessibility.captions.live_caption_bubble_expanded";

// Whether the Live Caption bubble is pinned.
inline constexpr char kLiveCaptionBubblePinned[] =
    "accessibility.captions.live_caption_bubble_pinned";

// Whether the Live Caption feature is enabled.
inline constexpr char kLiveCaptionEnabled[] =
    "accessibility.captions.live_caption_enabled";

// The language to use with the Live Caption feature.
inline constexpr char kLiveCaptionLanguageCode[] =
    "accessibility.captions.live_caption_language";

// Whether offensive words are masked.
inline constexpr char kLiveCaptionMaskOffensiveWords[] =
    "accessibility.captions.live_caption_mask_offensive_words";

// The list of origins that should not display an error message when using the
// Media Foundation renderer.
inline constexpr char kLiveCaptionMediaFoundationRendererErrorSilenced[] =
    "accessibility.captions.live_caption_media_foundation_renderer_error_"
    "silenced";

// This may be removed in the future but for now these preferences are ash only.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// Enables Captioning from microphone input.
inline constexpr char kLiveCaptionUserMicrophoneEnabled[] =
    "accessibility.captions.user_microphone_captioning_enabled";

// Describes the language code of the current locale for microphone input
// live captions.
inline constexpr char kUserMicrophoneCaptionLanguageCode[] =
    "accessibility.captions.user_microphone_language_code";

const std::string GetUserMicrophoneCaptionLanguage(PrefService* profile_prefs);
bool IsLanguageCodeForMicrophoneCaption(speech::LanguageCode language_code,
                                        PrefService* profile_prefs);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const std::string GetLiveCaptionLanguageCode(PrefService* profile_prefs);
bool IsLanguageCodeForLiveCaption(speech::LanguageCode language_code,
                                  PrefService* profile_prefs);

#endif  // !defined(ANDROID)

// These kAccessibilityCaptions* caption style prefs are used on Android
// (though their primary use is for Live Caption, which is why they are housed
// within this live_caption component instead of somewhere more generic).

// String indicating the size of the captions text as a percentage.
inline constexpr char kAccessibilityCaptionsTextSize[] =
    "accessibility.captions.text_size";

// String indicating the font of the captions text.
inline constexpr char kAccessibilityCaptionsTextFont[] =
    "accessibility.captions.text_font";

// Comma-separated string indicating the RGB values of the captions text color.
inline constexpr char kAccessibilityCaptionsTextColor[] =
    "accessibility.captions.text_color";

// Integer indicating the opacity of the captions text from 0 - 100.
inline constexpr char kAccessibilityCaptionsTextOpacity[] =
    "accessibility.captions.text_opacity";

// Comma-separated string indicating the RGB values of the background color.
inline constexpr char kAccessibilityCaptionsBackgroundColor[] =
    "accessibility.captions.background_color";

// CSS string indicating the shadow of the captions text.
inline constexpr char kAccessibilityCaptionsTextShadow[] =
    "accessibility.captions.text_shadow";

// Integer indicating the opacity of the captions text background from 0 - 100.
inline constexpr char kAccessibilityCaptionsBackgroundOpacity[] =
    "accessibility.captions.background_opacity";

// Live Translate prefs.

// Whether the Live Translate feature is enabled.
inline constexpr char kLiveTranslateEnabled[] =
    "accessibility.captions.live_translate_enabled";

// The target language to translate the captions to.
inline constexpr char kLiveTranslateTargetLanguageCode[] =
    "accessibility.captions.live_translate_target_language";

}  // namespace prefs

#endif  // COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_
