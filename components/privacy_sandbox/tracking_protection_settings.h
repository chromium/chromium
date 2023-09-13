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

// A service which provides an interface for observing and reading tracking
// protection settings.
class TrackingProtectionSettings : TrackingProtectionOnboarding::Observer,
                                   public KeyedService {
 public:
  class Observer {
   public:
    // For observation of DNT.
    virtual void OnDoNotTrackEnabledChanged() {}

    // For observation of block all 3PC.
    virtual void OnBlockAllThirdPartyCookiesChanged() {}
  };

  explicit TrackingProtectionSettings(
      PrefService* pref_service,
      TrackingProtectionOnboarding* onboarding_service);
  ~TrackingProtectionSettings() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Wrapper functions for kTrackingProtectionLevel pref.
  tracking_protection::TrackingProtectionLevel GetTrackingProtectionLevel()
      const;
  bool IsCustomTrackingProtectionLevel() const;
  bool IsStandardTrackingProtectionLevel() const;

  // Returns whether "do not track" is enabled.
  bool IsDoNotTrackEnabled() const;

  // Returns whether tracking protection for 3PCD (prefs + UX) is enabled.
  bool IsTrackingProtection3pcdEnabled() const;

  // Returns whether all 3PC are blocked (i.e. without mitigations).
  bool AreAllThirdPartyCookiesBlocked() const;

  // From TrackingProtectionOnboarding::Observer
  void OnTrackingProtectionOnboarded() override;

 private:
  // Callbacks for pref observation.
  void OnDoNotTrackEnabledPrefChanged();
  void OnTrackingProtectionLevelPrefChanged();
  void OnBlockAllThirdPartyCookiesPrefChanged();
  void OnTrackingProtection3pcdPrefChanged();

  base::ObserverList<Observer>::Unchecked observers_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;

  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
