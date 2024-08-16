// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

using content_settings::CookieControlsMode;

CookieControlsService::CookieControlsService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile->IsOffTheRecord());
  Init();
}

CookieControlsService::~CookieControlsService() = default;

void CookieControlsService::Init() {
  incognito_cookie_settings_ = CookieSettingsFactory::GetForProfile(profile_);
  cookie_observations_.AddObservation(incognito_cookie_settings_.get());
  regular_cookie_settings_ =
      CookieSettingsFactory::GetForProfile(profile_->GetOriginalProfile());
  cookie_observations_.AddObservation(regular_cookie_settings_.get());

  if (profile_->GetProfilePolicyConnector()) {
    policy_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
        profile_->GetProfilePolicyConnector()->policy_service(),
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
    policy_registrar_->Observe(
        policy::key::kBlockThirdPartyCookies,
        base::BindRepeating(
            &CookieControlsService::OnThirdPartyCookieBlockingPolicyChanged,
            base::Unretained(this)));
  }
}

void CookieControlsService::Shutdown() {
  cookie_observations_.RemoveAllObservations();
  policy_registrar_.reset();
}

void CookieControlsService::HandleCookieControlsToggleChanged(bool checked) {
  profile_->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(checked ? CookieControlsMode::kIncognitoOnly
                               : CookieControlsMode::kOff));
  base::RecordAction(
      checked ? base::UserMetricsAction("CookieControls.NTP.Enabled")
              : base::UserMetricsAction("CookieControls.NTP.Disabled"));
}

bool CookieControlsService::ShouldEnforceCookieControls() {
  return GetCookieControlsEnforcement() !=
         CookieControlsEnforcement::kNoEnforcement;
}

CookieControlsEnforcement
CookieControlsService::GetCookieControlsEnforcement() {
  auto* pref = profile_->GetPrefs()->FindPreference(prefs::kCookieControlsMode);
  if (pref->IsManaged())
    return CookieControlsEnforcement::kEnforcedByPolicy;
  if (pref->IsExtensionControlled())
    return CookieControlsEnforcement::kEnforcedByExtension;
  if (regular_cookie_settings_->ShouldBlockThirdPartyCookies()) {
    return CookieControlsEnforcement::kEnforcedByCookieSetting;
  }
  return CookieControlsEnforcement::kNoEnforcement;
}

bool CookieControlsService::GetToggleCheckedValue() {
  return incognito_cookie_settings_->ShouldBlockThirdPartyCookies();
}

void CookieControlsService::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  for (Observer& obs : observers_)
    obs.OnThirdPartyCookieBlockingPrefChanged();
}

void CookieControlsService::OnThirdPartyCookieBlockingPolicyChanged(
    const base::Value* previous,
    const base::Value* current) {
  for (Observer& obs : observers_)
    obs.OnThirdPartyCookieBlockingPolicyChanged();
}
