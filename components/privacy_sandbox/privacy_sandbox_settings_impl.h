// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

class HostContentSettingsMap;
class PrefService;

namespace content_settings {
class CookieSettings;
}
namespace privacy_sandbox_test_util {
class PrivacySandboxSettingsTestPeer;
}

namespace privacy_sandbox {

class PrivacySandboxSettingsImpl : public PrivacySandboxSettings {
 public:
  // Ideally the only external locations that call this constructor are the
  // factory, and dedicated tests.
  // TODO(crbug.com/40252892): Currently tests dedicated to other components
  // rely on this interface, they should be migrated to something better (such
  // as a dedicated test builder)
  PrivacySandboxSettingsImpl(
      HostContentSettingsMap* host_content_settings_map,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      PrefService* pref_service);
  ~PrivacySandboxSettingsImpl() override;

  // KeyedService:
  void Shutdown() override;

  // PrivacySandboxSettings:
  bool IsEventReportingDestinationAttested(
      const url::Origin& destination_origin,
      privacy_sandbox::PrivacySandboxAttestationsGatedAPI invoking_api)
      const override;
  bool IsSharedStorageAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      content::RenderFrameHost* console_frame,
      bool* out_block_is_site_setting_specific) const override;
  bool IsSharedStorageSelectURLAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific) const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  bool AreRelatedWebsiteSetsEnabled() const override;

 private:
  friend class PrivacySandboxAttestations;
  // NOTE: Do not add any new friend classes for testing; tests that need
  // access to private functions / variables should go through this peer class.
  friend class privacy_sandbox_test_util::PrivacySandboxSettingsTestPeer;

  // Called when the Related Website Sets enabled preference is changed.
  void OnRelatedWebsiteSetsEnabledPrefChanged();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kAllowed = 0,
    kRestricted = 1,
    kIncognitoProfile = 2,
    kApisDisabled = 3,
    kSiteDataAccessBlocked = 4,
    kMismatchedConsent = 5,
    kAttestationFailed = 6,
    kAttestationsFileNotYetReadyNOLONGERRECORDED = 7,
    kAttestationsDownloadedNotYetLoaded = 8,
    kAttestationsFileCorrupt = 9,
    kJoiningTopFrameBlocked = 10,
    kAttestationsFileNotYetChecked = 12,
    kAttestationsFileNotPresent = 13,
    kMaxValue = kAttestationsFileNotPresent,
  };

  base::ObserverList<Observer>::Unchecked observers_;

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
