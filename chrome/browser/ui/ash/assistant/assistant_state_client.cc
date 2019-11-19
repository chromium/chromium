// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_state_client.h"

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

AssistantStateClient::AssistantStateClient() {
  arc::ArcSessionManager::Get()->AddObserver(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

AssistantStateClient::~AssistantStateClient() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  arc::ArcSessionManager::Get()->RemoveObserver(this);
}

void AssistantStateClient::NotifyFeatureAllowed() {
  DCHECK(profile_);
  ash::mojom::AssistantAllowedState state =
      assistant::IsAssistantAllowedForProfile(profile_);
  ash::AssistantState::Get()->NotifyFeatureAllowed(state);
}

void AssistantStateClient::NotifyLocaleChanged() {
  DCHECK(profile_);

  NotifyFeatureAllowed();

  std::string out_locale =
      profile_->GetPrefs()->GetString(language::prefs::kApplicationLocale);

  ash::AssistantState::Get()->NotifyLocaleChanged(out_locale);
}

void AssistantStateClient::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&AssistantStateClient::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

void AssistantStateClient::OnArcPlayStoreEnabledChanged(bool enabled) {
  ash::AssistantState::Get()->NotifyArcPlayStoreEnabledChanged(enabled);
}

void AssistantStateClient::SetProfileByUser(const user_manager::User* user) {
  SetProfile(chromeos::ProfileHelper::Get()->GetProfileByUser(user));
}

void AssistantStateClient::SetProfile(Profile* profile) {
  if (profile_ == profile)
    return;

  profile_ = profile;
  pref_change_registrar_.reset();

  if (!profile_)
    return;

  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      language::prefs::kApplicationLocale,
      base::BindRepeating(&AssistantStateClient::NotifyLocaleChanged,
                          base::Unretained(this)));

  NotifyLocaleChanged();
  OnArcPlayStoreEnabledChanged(arc::IsArcPlayStoreEnabledForProfile(profile_));
}
