// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_translate_controller.h"

#include <memory>

#include "base/bind.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace captions {

LiveTranslateController::LiveTranslateController(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(
          &LiveTranslateController::OnLiveCaptionEnabledChanged,
          // Unretained is safe because |this| owns |pref_change_registrar_|.
          base::Unretained(this)));
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
  registry->RegisterBooleanPref(
      prefs::kLiveTranslateEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(prefs::kLiveTranslateTargetLanguageCode,
                               speech::kUsEnglishLocale,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void LiveTranslateController::OnLiveCaptionEnabledChanged() {
  if (!profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled))
    profile_prefs_->SetBoolean(prefs::kLiveTranslateEnabled, false);
}

void LiveTranslateController::OnLiveTranslateEnabledChanged() {
  if (profile_prefs_->GetBoolean(prefs::kLiveTranslateEnabled))
    profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, true);
}

}  // namespace captions
