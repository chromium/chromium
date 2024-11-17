// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_REALTIME_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_REALTIME_MECHANISM_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/database_manager_mechanism.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace safe_browsing {

// This performs the real-time URL Safe Browsing check.
class UrlRealTimeMechanism : public SafeBrowsingLookupMechanism {
 public:
  UrlRealTimeMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      SessionID tab_id,
      std::unique_ptr<SafeBrowsingLookupMechanism>
          hash_realtime_lookup_mechanism);
  UrlRealTimeMechanism(const UrlRealTimeMechanism&) = delete;
  UrlRealTimeMechanism& operator=(const UrlRealTimeMechanism&) = delete;
  ~UrlRealTimeMechanism() override;

 private:
  // SafeBrowsingLookupMechanism implementation:
  StartCheckResult StartCheckInternal() override;

  // If |did_match_allowlist| is true, this will fall back to the hash-based
  // check instead of performing the URL lookup.
  void OnCheckUrlForHighConfidenceAllowlist(
      bool did_match_allowlist,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // This function has to be static because it is called in UI thread.
  // This function starts a real time url check if |url_lookup_service_on_ui| is
  // available and is not in backoff mode. Otherwise, hop back to IO thread and
  // perform hash based check.
  static void StartLookupOnUIThread(
      base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
      const GURL& url,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      SessionID tab_id,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Checks the eligibility of sending a sampled ping first;
  // Send a sampled report if one should be sent, otherwise, exit.
  static void MaybeSendSampleRequest(
      base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
      const GURL& url,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      SessionID tab_id,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Called when the |response| from the real-time lookup service is received.
  // |is_lookup_successful| is true if the response code is OK and the
  // response body is successfully parsed.
  // |is_cached_response| is true if the response is a cache hit. In such a
  // case, fall back to hash-based checks if the cached verdict is |SAFE|.
  void OnLookupResponse(bool is_lookup_successful,
                        bool is_cached_response,
                        std::unique_ptr<RTLookupResponse> response);

  // Perform the hash based check for the url.
  void PerformHashBasedCheck(const GURL& url,
                             HashDatabaseFallbackTrigger fallback_trigger);

  // The real-time URL check can sometimes default back to the hash-based check.
  // In these cases, this function is called once the check has completed, so
  // that the real-time URL check can report back the final results to the
  // caller.
  void OnHashDatabaseCompleteCheckResult(
      HashDatabaseFallbackTrigger fallback_trigger,
      std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result);
  void OnHashDatabaseCompleteCheckResultInternal(
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      std::optional<ThreatSource> threat_source,
      HashDatabaseFallbackTrigger fallback_trigger);

  // This function is called once the hash-prefix real-time check has completed,
  // which which temporarily stores the results in this class for logging
  // purposes only.
  void OnHashRealTimeCompleteCheckResult(
      std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result);
  void OnHashRealTimeCompleteCheckResultInternal(SBThreatType threat_type);

  void MaybePerformSuspiciousSiteDetection(
      RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type);

  // This sends back the real-time URL lookup result to the caller.
  // Additionally, If a background HPRT was sent and the result returned in
  // time, this logs that result.
  void CompleteCheckInternal(
      std::unique_ptr<CompleteCheckResult> complete_check_result);

  SEQUENCE_CHECKER(sequence_checker_);

  // Whether safe browsing database can be checked. It is set to false when
  // enterprise real time URL lookup is enabled and safe browsing is disabled
  // for this profile.
  bool can_check_db_;

  // Whether the high confidence allowlist can be checked. It is set to
  // false when enterprise real time URL lookup is enabled.
  bool can_check_high_confidence_allowlist_;

  // URL Lookup service suffix for logging metrics.
  std::string url_lookup_service_metric_suffix_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform real time url check. Can only be
  // accessed in UI thread.
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui_;

  // This object is used to call the |NotifySusiciousSiteDetected| method on
  // URLs with suspicious verdicts.
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate_;

  // This stores the callback method that will be used to obtain WebContents,
  // which is needed to call the |NotifySusiciousSiteDetected| trigger.
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;

  // If the URL is classified as safe in cache manager during real time
  // lookup.
  bool is_cached_safe_url_ = false;

  // This will be created in cases where the real-time URL check decides to fall
  // back to the hash-based checks.
  std::unique_ptr<DatabaseManagerMechanism> hash_database_mechanism_ = nullptr;

  // The current tab ID. Used sometimes for identifying the referrer chain. Can
  // be |SessionID::InvalidValue()|.
  SessionID tab_id_;

  // Store the verdict from the HPRT lookup result in this class. This verdict
  // will be used when the URL real-time lookup completes.
  std::optional<SBThreatType> hash_realtime_lookup_result_threat_type_;

  // A helper method to log background HPRT related metrics and construct CSBRR
  // reports.
  void LogBackgroundHprtLookupResults(SBThreatType urt_threat_type);

  // Converts a |SBThreatType| to the one used for
  // the UrlRealTimeAndHashRealTimeDiscrepancyInfo threat type CSBRR.
  static ClientSafeBrowsingReportRequest::
      UrlRealTimeAndHashRealTimeDiscrepancyInfo::LookupThreatType
      GetDiscrepancyThreatType(SBThreatType threat_type);

  // This will be populated in cases where the sampled HPRT lookup should be
  // sent.
  std::unique_ptr<SafeBrowsingLookupMechanism> hash_realtime_lookup_mechanism_ =
      nullptr;

  base::OnceCallback<void(std::unique_ptr<ClientSafeBrowsingReportRequest>)>
      save_report_info_for_testing_;

  base::WeakPtrFactory<UrlRealTimeMechanism> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_REALTIME_MECHANISM_H_
