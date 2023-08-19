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

}  // namespace prefs
