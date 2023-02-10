// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_

#include "components/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"

class HostContentSettingsMap;
class PrefService;

namespace content_settings {
class CookieSettings;
}

namespace privacy_sandbox {

class PrivacySandboxSettingsImpl : public PrivacySandboxSettings {
 public:
  // Ideally the only external locations that call this constructor are the
  // factory, and dedicated tests.
  // TODO(crbug.com/1406840): Currently tests dedicated to other components rely
  // on this interface, they should be migrated to something better (such as a
  // dedicated test builder)
  PrivacySandboxSettingsImpl(
      std::unique_ptr<Delegate> delegate,
      HostContentSettingsMap* host_content_settings_map,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      PrefService* pref_service);
  ~PrivacySandboxSettingsImpl() override;

  // PrivacySandboxSettings:
  bool IsTopicsAllowed() const override;
  bool IsTopicsAllowedForContext(const url::Origin& top_frame_origin,
                                 const GURL& url) const override;
  bool IsTopicAllowed(const CanonicalTopic& topic) override;
  void SetTopicAllowed(const CanonicalTopic& topic, bool allowed) override;
  void ClearTopicSettings(base::Time start_time, base::Time end_time) override;
  base::Time TopicsDataAccessibleSince() const override;
  bool IsAttributionReportingEverAllowed() const override;
  bool IsAttributionReportingAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const override;
  bool MaySendAttributionReport(
      const url::Origin& source_origin,
      const url::Origin& destination_origin,
      const url::Origin& reporting_origin) const override;
  void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                               bool allowed) override;
  void ClearFledgeJoiningAllowedSettings(base::Time start_time,
                                         base::Time end_time) override;
  bool IsFledgeJoiningAllowed(
      const url::Origin& top_frame_origin) const override;
  bool IsFledgeAllowed(const url::Origin& top_frame_origin,
                       const url::Origin& auction_party) const override;
  bool IsSharedStorageAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) const override;
  bool IsSharedStorageSelectURLAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) const override;
  bool IsPrivateAggregationAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const override;
  bool IsPrivacySandboxEnabled() const override;
  void SetAllPrivacySandboxAllowedForTesting() override;
  void SetTopicsBlockedForTesting() override;
  void SetPrivacySandboxEnabled(bool enabled) override;
  bool IsTrustTokensAllowed() override;
  bool IsPrivacySandboxRestricted() const override;
  void OnCookiesCleared() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) override;

 private:
  friend class PrivacySandboxSettingsM1Test;
  // Called when the main privacy sandbox preference is changed.
  void OnPrivacySandboxPrefChanged();

  // Called when the First-Party Sets enabled preference is changed.
  void OnFirstPartySetsEnabledPrefChanged();

  // Determines based on the current features, preferences and provided
  // |cookie_settings| whether Privacy Sandbox APIs are generally allowable for
  // |url| on |top_frame_origin|. Individual APIs may perform additional checks
  // for allowability (such as incognito) on top of this. |cookie_settings| is
  // provided as a parameter to allow callers to cache it between calls.
  bool IsPrivacySandboxEnabledForContext(
      const absl::optional<url::Origin>& top_frame_origin,
      const GURL& url) const;

  void SetTopicsDataAccessibleFromNow() const;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kAllowed,
    kRestricted,
    kIncognitoProfile,
    kApisDisabled,
    kSiteDataAccessBlocked,
    kMismatchedConsent,
    kMaxValue = kMismatchedConsent,
  };

  static bool IsAllowed(Status status);

  // Whether the site associated with the URL is allowed to access privacy
  // sandbox APIs within the context of |top_frame_origin|.
  Status GetSiteAccessAllowedStatus(const url::Origin& top_frame_origin,
                                    const GURL& url) const;

  // Whether the privacy sandbox APIs can be allowed given the current
  // environment. For example, the privacy sandbox is always disabled in
  // Incognito and for restricted accounts.
  Status GetPrivacySandboxAllowedStatus() const;

  // Whether the privacy sandbox associated with  the |pref_name| is enabled.
  // For individual sites, check as well with GetSiteAccessAllowedStatus.
  Status GetM1PrivacySandboxApiEnabledStatus(
      const std::string& pref_name) const;

  // Whether the Topics API can be allowed given the current
  // environment or the reason why it is not allowed.
  Status GetM1TopicAllowedStatus() const;

  // Whether Attribution Reporting API can be allowed given the current
  // environment or the reason why it is not allowed.
  Status GetM1AttributionReportingAllowedStatus(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const;

  // Whether Fledge can be allowed given the current environment or the reason
  // why it is not allowed.
  Status GetM1FledgeAllowedStatus(const url::Origin& top_frame_origin,
                                  const url::Origin& accessing_origin) const;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<Delegate> delegate_;
  raw_ptr<HostContentSettingsMap, DanglingUntriaged> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
