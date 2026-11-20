// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
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
      const url::Origin& destination_origin) const override;

 private:
  friend class PrivacySandboxAttestations;
  // NOTE: Do not add any new friend classes for testing; tests that need
  // access to private functions / variables should go through this peer class.
  friend class privacy_sandbox_test_util::PrivacySandboxSettingsTestPeer;

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

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
