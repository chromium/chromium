// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Service which receives events from Trust & Safety features and determines
// whether or not to launch a HaTS survey on the NTP for the user.
class TrustSafetySentimentService : public KeyedService {
 public:
  explicit TrustSafetySentimentService(Profile* profile);
  ~TrustSafetySentimentService() override;

  // Called to indicate to the service that the user opened an NTP. This allows
  // the service to update its eligibility logic, and potentially show a
  // survey.
  void OpenedNewTabPage();

  // Called to indicate to the service that the user has interacted with the
  // privacy settings on chrome://settings in |web_contents|. Interaction in
  // this context could be using a link row on the privacy settings card.
  // Calling this allows the service to monitor |web_contents| to determine
  // if the user stays on settings for the required time.
  void InteractedWithPrivacySettings(content::WebContents* web_contents);

  // Called to indicate to the service that the user has run safety check. This
  // is immediately considered as a trigger action.
  void RanSafetyCheck();

  // The feature areas that the service delivers HaTS surveys for. Each feature
  // area is associated with a different Listnr survey, and has a different set
  // of Product Specific Data (PSD).
  enum class FeatureArea {
    kPrivacySettings = 0,
    kTrustedSurface,
    kTransactions
  };

 private:
  friend class TrustSafetySentimentServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           Eligibility_NtpOpens);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, Eligibility_Time);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, TriggerProbability);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           TriggersClearOnLaunch);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, SettingsWatcher);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, RanSafetyCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           PrivacySettingsProductSpecificData);

  // Struct representing a trigger (user action relevant to T&S) that previously
  // occurred, and is awaiting the appropriate eligibility steps before causing
  // a survey to be shown.
  struct PendingTrigger {
    PendingTrigger();
    PendingTrigger(const PendingTrigger& other);
    PendingTrigger(const std::map<std::string, bool>& product_specific_data,
                   int remaining_ntps_to_open);
    ~PendingTrigger();

    std::map<std::string, bool> product_specific_data;
    int remaining_ntps_to_open;
    base::Time occurred_time;
  };

  // Class which observes the provided |web_contents| for |required_open_time|
  // and then checks if |web_contents| is currently visible, and has settings
  // open.
  class SettingsWatcher : content::WebContentsObserver {
   public:
    SettingsWatcher(content::WebContents* web_contents,
                    base::TimeDelta required_open_time,
                    base::OnceCallback<void(bool)> complete_callback);
    ~SettingsWatcher() override;

    // WebContentsObserver:
    void WebContentsDestroyed() override;

   private:
    void TimerComplete();

    content::WebContents* web_contents_;
    base::OnceCallback<void(bool)> complete_callback_;
    base::WeakPtrFactory<SettingsWatcher> weak_ptr_factory_{this};
  };

  void SettingsWatcherComplete(bool stayed_on_settings);

  // Record that a trigger occurred, placing it in the set of pending triggers.
  // Private as the service itself determines when a trigger has occurred, and
  // is responsible for generating the appropriate |product_specific_data|.
  void TriggerOccurred(
      FeatureArea feature_area,
      const std::map<std::string, bool>& product_specific_data);

  Profile* const profile_;
  std::map<FeatureArea, PendingTrigger> pending_triggers_;
  std::unique_ptr<SettingsWatcher> settings_watcher_;
  base::WeakPtrFactory<TrustSafetySentimentService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
