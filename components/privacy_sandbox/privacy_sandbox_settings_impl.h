// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"

class HostContentSettingsMap;
class PrefService;

namespace content_settings {
class CookieSettings;
}

namespace privacy_sandbox {

class PrivacySandboxSettingsImpl : public PrivacySandboxSettings,
                                   public TrackingProtectionSettingsObserver {
 public:
  // Ideally the only external locations that call this constructor are the
  // factory, and dedicated tests.
  // TODO(crbug.com/40252892): Currently tests dedicated to other components
  // rely on this interface, they should be migrated to something better (such
  // as a dedicated test builder)
  PrivacySandboxSettingsImpl(
      std::unique_ptr<Delegate> delegate,
      HostContentSettingsMap* host_content_settings_map,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      TrackingProtectionSettings* tracking_protection_settings,
      PrefService* pref_service);
  ~PrivacySandboxSettingsImpl() override;

  // KeyedService:
  void Shutdown() override;

  // PrivacySandboxSettings:
  bool IsTopicsAllowed() const override;
  bool IsTopicsAllowedForContext(
      const url::Origin& top_frame_origin,
      const GURL& url,
      content::RenderFrameHost* console_frame = nullptr) const override;
  bool IsTopicAllowed(const CanonicalTopic& topic) override;
  void SetTopicAllowed(const CanonicalTopic& topic, bool allowed) override;
  bool IsTopicPrioritized(const CanonicalTopic& topic) override;
  void ClearTopicSettings(base::Time start_time, base::Time end_time) override;
  base::Time TopicsDataAccessibleSince() const override;
  bool IsAttributionReportingEverAllowed() const override;
  bool IsAttributionReportingAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      content::RenderFrameHost* console_frame = nullptr) const override;
  bool MaySendAttributionReport(
      const url::Origin& source_origin,
      const url::Origin& destination_origin,
      const url::Origin& reporting_origin,
      content::RenderFrameHost* console_frame = nullptr) const override;
  bool IsAttributionReportingTransitionalDebuggingAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      bool& can_bypass) const override;
  void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                               bool allowed) override;
  void ClearFledgeJoiningAllowedSettings(base::Time start_time,
                                         base::Time end_time) override;
  bool IsFledgeAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& auction_party,
      content::InterestGroupApiOperation interest_group_api_operation,
      content::RenderFrameHost* console_frame = nullptr) const override;
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
  bool IsLocalUnpartitionedDataAccessAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      content::RenderFrameHost* console_frame) const override;
  bool IsPrivateAggregationAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      bool* out_block_is_site_setting_specific) const override;
  bool IsPrivateAggregationDebugModeAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const override;
  TpcdExperimentEligibility GetCookieDeprecationExperimentCurrentEligibility()
      const override;

  bool IsCookieDeprecationLabelAllowed() const override;
  bool IsCookieDeprecationLabelAllowedForContext(
      const url::Origin& top_frame_origin,
      const url::Origin& context_origin) const override;
  void SetAllPrivacySandboxAllowedForTesting() override;
  void SetTopicsBlockedForTesting() override;
  bool IsPrivacySandboxRestricted() const override;
  bool IsPrivacySandboxCurrentlyUnrestricted() const override;
  bool IsSubjectToM1NoticeRestricted() const override;
  bool IsRestrictedNoticeEnabled() const override;
  void OnCookiesCleared() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) override;

  bool AreRelatedWebsiteSetsEnabled() const override;

 private:
  // TODO(crbug.com/366168654): Browser tests should not reach into the private
  // method or states of this class. Consider exposing the required functions
  // via a test helper class or test only functions.
  friend class PrivacySandboxSettingsTest;
  friend class PrivacySandboxAttestations;
  friend class PrivacySandboxAttestationsTestBase;
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationsBrowserTest,
      CallComponentReadyWhenRegistrationFindsExistingComponent);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxAttestationsBrowserTest,
                           SentinelFilePreventsSubsequentParsings);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxAttestationsBrowserTest,
                           DifferentHistogramAfterAttestationsFileCheck);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationPreInstallInteractionWithDownloadTest,
      BothPreinstalledAndDownloadedAttestationsAvailable);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest, FledgeJoiningAllowed);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest, NonEtldPlusOneBlocked);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxSettingsTest,
                           FledgeJoinSettingTimeRangeDeletion);
  // Called when the Related Website Sets enabled preference is changed.
  void OnRelatedWebsiteSetsEnabledPrefChanged();

  // Called when the CookieControlsMode preference is changed.
  void OnCookieControlsModePrefChanged();

  void SetTopicsDataAccessibleFromNow() const;

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
    kBlockedBy3pcdExperiment = 11,
    kAttestationsFileNotYetChecked = 12,
    kAttestationsFileNotPresent = 13,
    kMaxValue = kAttestationsFileNotPresent,
  };

  static bool IsAllowed(Status status);

  static void JoinHistogram(const char* name, Status status);
  static void JoinFledgeHistogram(
      content::InterestGroupApiOperation interest_group_api_operation,
      Status status);

  // Get the Topics that are disabled by Finch.
  const std::set<browsing_topics::Topic>& GetFinchDisabledTopics();

  // Get the Topics that are prioritized for top topic selection by Finch.
  const std::set<browsing_topics::Topic>& GetFinchPrioritizedTopics();

  // Whether the site associated with the URL is allowed to access privacy
  // sandbox APIs within the context of |top_frame_origin|.
  Status GetSiteAccessAllowedStatus(const url::Origin& top_frame_origin,
                                    const GURL& url) const;

  // Whether the privacy sandbox APIs can be allowed given the current
  // environment. For example, the privacy sandbox is always disabled in
  // Incognito and for restricted accounts.
  Status GetPrivacySandboxAllowedStatus(
      bool should_ignore_restriction = false) const;

  // Whether the privacy sandbox associated with  the |pref_name| is enabled.
  // For individual sites, check as well with GetSiteAccessAllowedStatus.
  Status GetM1PrivacySandboxApiEnabledStatus(
      const std::string& pref_name) const;

  // Whether the Topics API can be allowed given the current
  // environment or the reason why it is not allowed.
  Status GetM1TopicAllowedStatus() const;

  // Whether ad measurement APIs can be allowed given the current environment or
  // the reason why it is not allowed.
  Status GetM1AdMeasurementAllowedStatus(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const;

  // Whether Fledge can be allowed given the current environment or the reason
  // why it is not allowed.
  Status GetM1FledgeAllowedStatus(const url::Origin& top_frame_origin,
                                  const url::Origin& accessing_origin) const;

  // Internal helper for `IsFledgeAllowed`. Used only when
  // `interest_group_api_operation` is `kJoin`.
  bool IsFledgeJoiningAllowed(const url::Origin& top_frame_origin) const;

  // Whether fenced frame local unpartitioned data access is enabled.
  Status GetLocalUnpartitionedDataAccessEnabledStatus() const;

  // From TrackingProtectionSettingsObserver.
  void OnBlockAllThirdPartyCookiesChanged() override;

  // Sets the out parameter `out_block_is_site_setting_specific` if it is
  // non-null, based on the given `status`.
  void SetOutBlockIsSiteSettingSpecificFromStatus(
      Status status,
      bool* out_block_is_site_setting_specific) const;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<Delegate> delegate_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<TrackingProtectionSettings> tracking_protection_settings_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<TrackingProtectionSettings,
                          TrackingProtectionSettingsObserver>
      tracking_protection_settings_observation_{this};

  // Which topics are disabled by Finch; This is set and read by
  // GetFinchDisabledTopics.
  std::set<browsing_topics::Topic> finch_disabled_topics_;

  // Which topics are prioritized in top topic selection by Finch. This is set
  // and read by GetFinchPrioritizedTopics.
  std::set<browsing_topics::Topic> finch_prioritized_topics_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_IMPL_H_
