// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_translate_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/google_api_keys.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace captions {

LiveTranslateController::LiveTranslateController(
    PrefService* profile_prefs,
    content::BrowserContext* browser_context)
    : profile_prefs_(profile_prefs),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()),
      translation_dispatcher_(
          std::make_unique<TranslationDispatcher>(google_apis::GetAPIKey(),
                                                  browser_context)) {
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kLiveTranslateEnabled,
      base::BindRepeating(
          &LiveTranslateController::OnLiveTranslateEnabledChanged,
          // Unretained is safe because |this| owns |pref_change_registrar_|.
          base::Unretained(this)));
}

LiveTranslateController::~LiveTranslateController() = default;
// static
void LiveTranslateController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kLiveTranslateEnabled, false);

  registry->RegisterStringPref(prefs::kLiveTranslateTargetLanguageCode,
                               speech::kEnglishLocaleNoCountry);
}

void LiveTranslateController::GetTranslation(
    const std::string& result,
    std::string source_language,
    std::string target_language,
    OnTranslateEventCallback callback) {
  translation_dispatcher_->GetTranslation(result, source_language,
                                          target_language, std::move(callback));
}

void LiveTranslateController::OnLiveTranslateEnabledChanged() {
  if (profile_prefs_->GetBoolean(prefs::kLiveTranslateEnabled))
    profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, true);
}

}  // namespace captions
