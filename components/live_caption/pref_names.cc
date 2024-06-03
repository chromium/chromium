// Copyright 2020 The Chromium Authors
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

namespace {

const std::string GetCaptionLanguageCodeForPref(const std::string& pref,
                                                PrefService* profile_prefs) {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    return profile_prefs->GetString(pref);
  }

  return speech::kUsEnglishLocale;
}

}  // namespace

const std::string GetLiveCaptionLanguageCode(PrefService* profile_prefs) {
  return GetCaptionLanguageCodeForPref(prefs::kLiveCaptionLanguageCode,
                                       profile_prefs);
}

bool IsLanguageCodeForLiveCaption(speech::LanguageCode language_code,
                                  PrefService* profile_prefs) {
  return language_code ==
         speech::GetLanguageCode(GetLiveCaptionLanguageCode(profile_prefs));
}

#endif  // !defined(ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::string GetUserMicrophoneCaptionLanguage(PrefService* profile_prefs) {
  return GetCaptionLanguageCodeForPref(
      prefs::kUserMicrophoneCaptionLanguageCode, profile_prefs);
}

bool IsLanguageCodeForMicrophoneCaption(speech::LanguageCode language_code,
                                        PrefService* profile_prefs) {
  return language_code == speech::GetLanguageCode(
                              GetUserMicrophoneCaptionLanguage(profile_prefs));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace prefs
