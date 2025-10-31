// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

class GURL;
class HostContentSettingsMap;
class PrefService;

namespace content_settings {
struct SettingInfo;
}  // namespace content_settings

namespace policy {
class ManagementService;
}  // namespace policy

namespace syncer {
class SyncService;
}  // namespace syncer

namespace privacy_sandbox {

class TrackingProtectionSettingsObserver;

inline bool IsTrackingProtectionsUi(CookieControlsState controls_state) {
  return controls_state == CookieControlsState::kActiveTp ||
         controls_state == CookieControlsState::kPausedTp;
}

// Attempts to set prefs in order to roll back Mode B.
void MaybeSetRollbackPrefsModeB(syncer::SyncService* sync_service,
                                PrefService* prefs);

// A service which provides an interface for observing and reading tracking
// protection settings.
class TrackingProtectionSettings : public KeyedService {
 public:
  explicit TrackingProtectionSettings(
      PrefService* pref_service,
      HostContentSettingsMap* host_content_settings_map,
      policy::ManagementService* management_service,
      bool is_incognito);
  ~TrackingProtectionSettings() override;

  // KeyedService:
  void Shutdown() override;

  void AddObserver(TrackingProtectionSettingsObserver* observer);
  void RemoveObserver(TrackingProtectionSettingsObserver* observer);

  // Returns whether tracking protection for 3PCD (prefs + UX) is enabled.
  bool IsTrackingProtection3pcdEnabled() const;

  // Returns whether tracking protection 3PCD is enabled and all 3PC are blocked
  // (i.e. without mitigations).
  bool AreAllThirdPartyCookiesBlocked() const;

  // Returns whether IP protection is enabled.
  bool IsIpProtectionEnabled() const;

  // Returns whether fingerprinting protection is enabled.
  bool IsFpProtectionEnabled() const;

  // Adds a site-scoped TRACKING_PROTECTION content setting equal to ALLOW for
  // `first_party_url`.
  void AddTrackingProtectionException(const GURL& first_party_url);

  // Resets the TRACKING_PROTECTION content setting for `first_party_url`.
  // Can reset both site-scoped (wildcarded) and origin-scoped exceptions.
  void RemoveTrackingProtectionException(const GURL& first_party_url);

  // Returns true if the user has a TRACKING_PROTECTION content setting equal to
  // ALLOW, indicating ACT features should be disabled on `first_party_url`.
  // NOTE: the default for TRACKING_PROTECTION is BLOCK and cannot be changed,
  // meaning this function will only return true for site-level content settings
  // (i.e. exceptions). To check whether individual ACT features are
  // enabled/disabled please use the functions specific to those features.
  bool HasTrackingProtectionException(
      const GURL& first_party_url,
      content_settings::SettingInfo* info = nullptr) const;

  // Returns whether IP protection is disabled, either because an enterprise
  // policy has been set that disables the feature or, when the
  // `kIpPrivacyDisableForEnterpriseByDefault` feature is enabled, because no
  // policy value has been set via enterprise policy and this is a managed
  // profile or client.
  bool IsIpProtectionDisabledForEnterprise();

 private:
  void OnEnterpriseControlForPrefsChanged();

  // Callbacks for pref observation.
  void OnBlockAllThirdPartyCookiesPrefChanged();
  void OnTrackingProtection3pcdPrefChanged();
  void OnIpProtectionPrefChanged();

  base::ObserverList<TrackingProtectionSettingsObserver>::Unchecked observers_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<policy::ManagementService> management_service_;

  bool is_incognito_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
