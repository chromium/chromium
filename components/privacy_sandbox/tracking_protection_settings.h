// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

class PrefService;

namespace privacy_sandbox {

class TrackingProtectionSettingsObserver;

// A service which provides an interface for observing and reading tracking
// protection settings.
class TrackingProtectionSettings
    : public TrackingProtectionOnboarding::Observer,
      public KeyedService {
 public:
  explicit TrackingProtectionSettings(
      PrefService* pref_service,
      TrackingProtectionOnboarding* onboarding_service,
      bool is_incognito);
  ~TrackingProtectionSettings() override;

  virtual void AddObserver(TrackingProtectionSettingsObserver* observer);
  virtual void RemoveObserver(TrackingProtectionSettingsObserver* observer);

  // Returns whether "do not track" is enabled.
  bool IsDoNotTrackEnabled() const;

  // Returns whether tracking protection for 3PCD (prefs + UX) is enabled.
  bool IsTrackingProtection3pcdEnabled() const;

  // Returns whether tracking protection 3PCD is enabled and all 3PC are blocked
  // (i.e. without mitigations).
  bool AreAllThirdPartyCookiesBlocked() const;

  // From TrackingProtectionOnboarding::Observer
  void OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus onboarding_status)
      override;

 private:
  void OnEnterpriseControlForPrefsChanged();

  // Callbacks for pref observation.
  void OnDoNotTrackEnabledPrefChanged();
  void OnBlockAllThirdPartyCookiesPrefChanged();
  void OnTrackingProtection3pcdPrefChanged();

  base::ObserverList<TrackingProtectionSettingsObserver>::Unchecked observers_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
  bool is_incognito_;

  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
