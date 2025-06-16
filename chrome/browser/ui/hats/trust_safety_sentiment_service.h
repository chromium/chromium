// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_interface.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using PasswordProtectionUIType = safe_browsing::WarningUIType;
using PasswordProtectionUIAction = safe_browsing::WarningAction;

inline constexpr base::TimeDelta kPasswordChangeInactivity = base::Minutes(30);
inline constexpr base::TimeDelta kSafetyHubSurveyDelay = base::Minutes(10);

// Service which receives events from Trust & Safety features and determines
// whether or not to launch a HaTS survey on the NTP for the user.
class TrustSafetySentimentService
    : public TrustSafetySentimentServiceInterface,
      public KeyedService,
      public ProfileObserver,
      public metrics::DesktopSessionDurationTracker::Observer {
 public:
  explicit TrustSafetySentimentService(Profile* profile);
  ~TrustSafetySentimentService() override;

  // TrustSafetySentimentServiceInterface:
  void OpenedNewTabPage() override;
  void InteractedWithPrivacySettings(
      content::WebContents* web_contents) override;
  void RanSafetyCheck() override;
  void PageInfoOpened() override;
  void InteractedWithPageInfo() override;
  void SavedPassword() override;
  void PageInfoClosed() override;
  void OpenedPasswordManager(content::WebContents* web_contents) override;
  void SavedCard() override;
  void RanPasswordCheck() override;
  void ClearedBrowsingData(browsing_data::BrowsingDataType datatype) override;
  void FinishedPrivacyGuide() override;
  void InteractedWithSafeBrowsingInterstitial(
      bool did_proceed,
      safe_browsing::SBThreatType threat_type) override;
  void InteractedWithDownloadWarningUI(
      DownloadItemWarningData::WarningSurface surface,
      DownloadItemWarningData::WarningAction action) override;
  void ProtectResetOrCheckPasswordClicked(
      PasswordProtectionUIType ui_type) override;
  void PhishedPasswordUpdateNotClicked(
      PasswordProtectionUIType ui_type,
      PasswordProtectionUIAction action) override;
  void PhishedPasswordUpdateFinished() override;
  void TriggerSafetyHubSurvey(
      TrustSafetySentimentService::FeatureArea feature_area,
      std::map<std::string, bool> product_specific_data) override;

  // Profile Observer:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // metrics::DesktopSessionDurationTracker::Observer:
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

  // Checks that this feature area is valid for the current version.
  static bool VersionCheck(FeatureArea feature_area);

  // Gets the HaTS trigger for a feature area.
  static std::string GetHatsTriggerForFeatureArea(FeatureArea feature_area);

  // Performs a FeatureArea and Version-specific dice roll.
  // Returns true if succeeds, else false.
  static bool ProbabilityCheck(FeatureArea feature_area);

 private:
  friend class TrustSafetySentimentServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           Eligibility_NtpOpens);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, Eligibility_Time);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, TriggerProbability);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           TriggersClearOnLaunch);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           SettingsWatcher_PrivacySettings);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           SettingsWatcher_PasswordManager);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, RanSafetyCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           PrivacySettingsProductSpecificData);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           Eligibility_V1FeatureWhileV2Enabled);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_SafetyCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_TrustedSurface);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_PasswordCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_BrowsingData);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           V2_BrowsingData_NotInterested);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_PrivacyGuide);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_ControlGroup);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           SafetyHubInteractionState);

  // Struct representing a trigger (user action relevant to T&S) that previously
  // occurred, and is awaiting the appropriate eligibility steps before causing
  // a survey to be shown.
  struct PendingTrigger {
    PendingTrigger();
    PendingTrigger(const PendingTrigger& other);
    PendingTrigger(const std::map<std::string, bool>& product_specific_data,
                   int remaining_ntps_to_open);
    explicit PendingTrigger(int remaining_ntps_to_open);
    ~PendingTrigger();

    std::map<std::string, bool> product_specific_data;
    int remaining_ntps_to_open;
    base::Time occurred_time;
  };

  // Class which observes the provided |web_contents| for |required_open_time|
  // and then checks if |web_contents| is currently visible, and has settings
  // open. Calls |success_callback| if the user stays on settings for the
  // required time, calls |complete_callback| when the observation time has
  // expired, or |web_contents| has been destroyed.
  class SettingsWatcher : content::WebContentsObserver {
   public:
    SettingsWatcher(content::WebContents* web_contents,
                    base::TimeDelta required_open_time,
                    base::OnceCallback<void()> success_callback,
                    base::OnceCallback<void()> complete_callback);
    ~SettingsWatcher() override;

    // WebContentsObserver:
    void WebContentsDestroyed() override;

   private:
    void TimerComplete();

    raw_ptr<content::WebContents> web_contents_;
    base::OnceCallback<void()> success_callback_;
    base::OnceCallback<void()> complete_callback_;
    base::WeakPtrFactory<SettingsWatcher> weak_ptr_factory_{this};
  };

  // Struct which represents the PageInfo state of interest to the service.
  struct PageInfoState {
    PageInfoState();
    base::Time opened_time;
    bool interacted = false;
  };

  // Struct which represents the PhishedPasswordChange state. When a user clicks
  // to change their password, we want to wait to trigger a survey until after
  // they change their password or the user has been inactive for some time.
  struct PhishedPasswordChangeState {
    PhishedPasswordChangeState();
    base::Time password_change_click_ts_;
    PasswordProtectionUIType ui_type_;
    bool finished_action = false;
  };

  void SettingsWatcherComplete();

  // Record that a trigger occurred, placing it in the set of pending triggers.
  // Private as the service itself determines when a trigger has occurred, and
  // is responsible for generating the appropriate |product_specific_data|.
  void TriggerOccurred(
      FeatureArea feature_area,
      const std::map<std::string, bool>& product_specific_data);

  // Record that the user performed an action which should make them temporarily
  // ineligible to receive a survey. This records a trigger for the kIneligible
  // feature area, which just like any other trigger will prevent a survey from
  // being shown, but will not result in a survey.
  void PerformedIneligibleAction();

  static bool ShouldBlockSurvey(const PendingTrigger& trigger);

  // Called by |ProtectResetOrCheckPasswordClicked| and
  // |PhishedPasswordUpdateNotClicked|. Triggers a survey if one has not already
  // been triggered for the user journey.
  void MaybeTriggerPasswordProtectionSurvey(PasswordProtectionUIType ui_type,
                                            PasswordProtectionUIAction action);

  const raw_ptr<Profile> profile_;
  std::map<FeatureArea, PendingTrigger> pending_triggers_;
  std::unique_ptr<SettingsWatcher> settings_watcher_;
  std::unique_ptr<PageInfoState> page_info_state_;
  std::unique_ptr<PhishedPasswordChangeState> phished_password_change_state_;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  bool performed_control_group_dice_roll_;
  base::WeakPtrFactory<TrustSafetySentimentService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
