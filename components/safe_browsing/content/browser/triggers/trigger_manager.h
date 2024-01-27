// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_TRIGGER_MANAGER_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/content/browser/triggers/trigger_throttler.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {

class BaseUIManager;
class ThreatDetails;

// A wrapper around different kinds of data collectors that can be active on a
// given browser tab. Any given field can be null or empty if the associated
// data is not being collected.
struct DataCollectorsContainer {
 public:
  DataCollectorsContainer();

  DataCollectorsContainer(const DataCollectorsContainer&) = delete;
  DataCollectorsContainer& operator=(const DataCollectorsContainer&) = delete;

  ~DataCollectorsContainer();

  // Note: new data collection types should be added below as additional fields.

  // Collects ThreatDetails which contains resource URLs and partial DOM.
  std::unique_ptr<ThreatDetails> threat_details;
};

// Stores the data collectors that are active on each WebContents (ie: browser
// tab). Keys are derived from WebContents* but should not be dereferenced.
using DataCollectorsMap = std::unordered_map<WebContentsKey,
                                             DataCollectorsContainer,
                                             typename WebContentsKey::Hasher>;

using SBErrorOptions =
    security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions;

// The reasons that trigger manager fails to create or finish a report.
// These values are written to logs. New enum values can be added, but
// existing enums must never be renumbered or deleted and reused.
enum class TriggerManagerReason {
  // Default value, used when there is no failure.
  NO_REASON = 0,
  // User preferences do not allow the report to be started or finished.
  USER_PREFERENCES = 1,
  // A report is already started on this tab, so no new report is started.
  REPORT_ALREADY_STARTED = 2,
  // There is no report to finish on this tab.
  NO_REPORT_TO_FINISH = 3,
  // No report is started because the user has exceeded their daily quota.
  DAILY_QUOTA_EXCEEDED = 4,
  // The report type has been deprecated so report can't be sent.
  REPORT_TYPE_DEPRECATED = 5,
  // New reasons must be added before kMaxValue and the value of kMaxValue
  // updated.
  kMaxValue = REPORT_TYPE_DEPRECATED
};

// This class manages SafeBrowsing data-reporting triggers. Triggers are
// activated for users opted-in to Extended Reporting and when security-related
// data collection is required.
//
// The TriggerManager has two main responsibilities: 1) ensuring triggers only
// run when appropriate, by honouring user opt-ins and incognito state, and 2)
// tracking how often triggers fire and throttling them when necessary.
class TriggerManager {
 public:
  struct FinishCollectingThreatDetailsResult {
    FinishCollectingThreatDetailsResult(bool should_send_report,
                                        bool are_threat_details_available);
    bool IsReportSent();
    bool should_send_report;
    bool are_threat_details_available;
  };

  TriggerManager(BaseUIManager* ui_manager, PrefService* local_state_prefs);

  TriggerManager(const TriggerManager&) = delete;
  TriggerManager& operator=(const TriggerManager&) = delete;

  virtual ~TriggerManager();

  // Returns a SBErrorDisplayOptions struct containing user state that is
  // relevant for TriggerManager to decide whether to start/finish data
  // collection. Looks at incognito state from |web_contents|, and opt-ins from
  // |pref_service|. Only the fields needed by TriggerManager will be set.
  static SBErrorOptions GetSBErrorDisplayOptions(
      const PrefService& pref_service,
      content::WebContents* web_contents);

  // Returns whether data collection can be started for the |trigger_type| based
  // on the settings specified in |error_display_options| as well as quota.
  // If false is returned, |out_reason| will be specify the reason.
  bool CanStartDataCollectionWithReason(
      const SBErrorOptions& error_display_options,
      const TriggerType trigger_type,
      TriggerManagerReason* out_reason);

  // Simplified signature for |CanStartDataCollectionWithReason| for callers
  // that don't care about the reason.
  bool CanStartDataCollection(const SBErrorOptions& error_display_options,
                              const TriggerType trigger_type);

  // Begins collecting a ThreatDetails report on the specified |web_contents|.
  // |resource| is the unsafe resource that cause the collection to occur.
  // |url_loader_factory| is used to retrieve data from the HTTP cache.
  // |history_service| is used to get data about redirects.
  // |error_display_options| contains the current state of relevant user
  // preferences. We use this object for interop with WebView, in Chrome it
  // should be created by TriggerManager::GetSBErrorDisplayOptions().
  // Returns true if the collection began, or false if it didn't.
  // If false is returned, |out_reason| is set to the reason the report didn't
  // start.
  virtual bool StartCollectingThreatDetailsWithReason(
      TriggerType trigger_type,
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      const SBErrorOptions& error_display_options,
      TriggerManagerReason* out_reason);

  // Simplified signature for |StartCollectingThreatDetailsWithReason| for
  // callers that don't care about the reason.
  virtual bool StartCollectingThreatDetails(
      TriggerType trigger_type,
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      const SBErrorOptions& error_display_options);

  // Store map of security interstitial interactions that should be sent in the
  // threat report.
  void SetInterstitialInteractions(
      std::unique_ptr<security_interstitials::InterstitialInteractionMap>
          interstitial_interactions);

  // Completes the collection of a ThreatDetails report for the specified
  // |web_contents_key| (derived from a WebContents*) and sends the
  // report. |delay| can be used to wait a period of time before finishing the
  // report. |did_proceed| indicates whether the user proceeded through the
  // security interstitial associated with this report. |num_visits| is how many
  // times the user has visited the site before. |error_display_options|
  // contains the current state of relevant user preferences.
  // We use this object for interop with WebView, in Chrome it should be
  // created by TriggerManager::GetSBErrorDisplayOptions(). |is_hats_candidate|
  // indicates whether the user is a candidate for a HaTS survey, in which case
  // this method will trigger launching it and attaching ThreatDetails report
  // information to it. Returns whether the report is supposed to be sent (eg:
  // is user  opted-in to extended reporting after collection began) and
  // whether the threat details were available to send.
  virtual FinishCollectingThreatDetailsResult FinishCollectingThreatDetails(
      TriggerType trigger_type,
      WebContentsKey web_contents_key,
      const base::TimeDelta& delay,
      bool did_proceed,
      int num_visits,
      const SBErrorOptions& error_display_options,
      std::optional<int64_t> warning_shown_ts = std::nullopt,
      bool is_hats_candidate = false);

  // Called when a ThreatDetails report finishes for the specified
  // |web_contents|.
  void ThreatDetailsDone(WebContentsKey web_contents_key);

  // Called when the specified |web_contents| is being destroyed. Used to clean
  // up our map by removing the corresponding WebContentsKey from the map.
  void WebContentsDestroyed(content::WebContents* web_contents);

 private:
  friend class TriggerManagerTest;

  // For testing only - allows injecting a mock Throttler.
  void set_trigger_throttler(TriggerThrottler* throttler);

  // The UI manager is used to send reports to Google. Not owned.
  // TODO(lpz): we may only need a the PingManager here.
  raw_ptr<BaseUIManager> ui_manager_;

  // Map of the data collectors running on each tabs. New keys are added the
  // first time any trigger tries to collect data on a tab and are removed when
  // the tab is destroyed. The values can be null if a trigger has finished on
  // a tab but the tab remains open.
  DataCollectorsMap data_collectors_map_;

  // Keeps track of how often triggers fire and throttles them when needed.
  std::unique_ptr<TriggerThrottler> trigger_throttler_;

  // Keeps track of user interactions with a security interstitial.
  std::unique_ptr<security_interstitials::InterstitialInteractionMap>
      interstitial_interactions_;

  base::WeakPtrFactory<TriggerManager> weak_factory_{this};
  // WeakPtrFactory should be last, don't add any members below it.
};

// A helper class that listens for events happening on a WebContents and can
// notify TriggerManager of any that are relevant.
class TriggerManagerWebContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TriggerManagerWebContentsHelper> {
 public:
  ~TriggerManagerWebContentsHelper() override;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<TriggerManagerWebContentsHelper>;

  TriggerManagerWebContentsHelper(content::WebContents* web_contents,
                                  TriggerManager* trigger_manager);

  // Trigger Manager will be notified of any relevant WebContents events.
  raw_ptr<TriggerManager> trigger_manager_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_TRIGGER_MANAGER_H_
