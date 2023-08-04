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
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

// This performs the real-time URL Safe Browsing check.
class UrlRealTimeMechanism : public SafeBrowsingLookupMechanism {
 public:
  // Interface via which a client of this class can surface relevant events in
  // WebUI. All methods must be called on the UI thread.
  class WebUIDelegate {
   public:
    virtual ~WebUIDelegate() = default;

    // Adds the new ping to the set of URT lookup pings. Returns a token that
    // can be used in |AddToURTLookupResponses| to correlate a ping and
    // response.
    virtual int AddToURTLookupPings(const RTLookupRequest request,
                                    const std::string oauth_token) = 0;

    // Adds the new response to the set of URT lookup pings.
    virtual void AddToURTLookupResponses(int token,
                                         const RTLookupResponse response) = 0;
  };

  UrlRealTimeMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      network::mojom::RequestDestination request_destination,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      const GURL& last_committed_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      WebUIDelegate* webui_delegate,
      MechanismExperimentHashDatabaseCache experiment_cache_selection);
  UrlRealTimeMechanism(const UrlRealTimeMechanism&) = delete;
  UrlRealTimeMechanism& operator=(const UrlRealTimeMechanism&) = delete;
  ~UrlRealTimeMechanism() override;

 private:
  // SafeBrowsingLookupMechanism implementation:
  StartCheckResult StartCheckInternal() override;

  // If |did_match_allowlist| is true, this will fall back to the hash-based
  // check instead of performing the URL lookup.
  void OnCheckUrlForHighConfidenceAllowlist(bool did_match_allowlist);

  // This function has to be static because it is called in UI thread.
  // This function starts a real time url check if |url_lookup_service_on_ui| is
  // available and is not in backoff mode. Otherwise, hop back to IO thread and
  // perform hash based check.
  static void StartLookupOnUIThread(
      base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Checks the eligibility of sending a sampled ping first;
  // Send a sampled report if one should be sent, otherwise, exit.
  static void MaybeSendSampleRequest(
      base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Called when the |request| from the real-time lookup service is sent.
  void OnLookupRequest(std::unique_ptr<RTLookupRequest> request,
                       std::string oauth_token);

  // Called when the |response| from the real-time lookup service is received.
  // |is_lookup_successful| is true if the response code is OK and the
  // response body is successfully parsed.
  // |is_cached_response| is true if the response is a cache hit. In such a
  // case, fall back to hash-based checks if the cached verdict is |SAFE|.
  void OnLookupResponse(bool is_lookup_successful,
                        bool is_cached_response,
                        std::unique_ptr<RTLookupResponse> response);

  // Logs |request| and |oauth_token| on any open chrome://safe-browsing pages.
  void LogLookupRequest(const RTLookupRequest& request,
                        const std::string& oauth_token);

  // Logs |response| on any open chrome://safe-browsing pages.
  void LogLookupResponse(const RTLookupResponse& response);

  void SetWebUIToken(int token);

  // Perform the hash based check for the url. |real_time_request_failed|
  // specifies whether this was triggered due to the real-time request having
  // failed (e.g. due to backoff, network errors, other service unavailability).
  void PerformHashBasedCheck(const GURL& url, bool real_time_request_failed);

  // The real-time URL check can sometimes default back to the hash-based check.
  // In these cases, this function is called once the check has completed, so
  // that the real-time URL check can report back the final results to the
  // caller.
  // |real_time_request_failed| specifies whether the real-time request failed
  // (e.g. due to backoff, network errors, other service unavailability).
  void OnHashDatabaseCompleteCheckResult(
      bool real_time_request_failed,
      std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result);
  void OnHashDatabaseCompleteCheckResultInternal(
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      absl::optional<ThreatSource> threat_source,
      bool real_time_request_failed);

  SEQUENCE_CHECKER(sequence_checker_);

  // This is used only for logging purposes, primarily (but not exclusively) to
  // distinguish between mainframe and non-mainframe resources.
  const network::mojom::RequestDestination request_destination_;

  // Token used for displaying url real time lookup pings. A single token is
  // sufficient since real time check only happens on main frame url.
  int url_web_ui_token_ = -1;

  // Whether safe browsing database can be checked. It is set to false when
  // enterprise real time URL lookup is enabled and safe browsing is disabled
  // for this profile.
  bool can_check_db_;

  // Whether the high confidence allowlist can be checked. It is set to
  // false when enterprise real time URL lookup is enabled.
  bool can_check_high_confidence_allowlist_;

  // Stores the response of
  // SafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist iff it was
  // called.
  bool did_match_allowlist_ = false;

  // URL Lookup service suffix for logging metrics.
  std::string url_lookup_service_metric_suffix_;

  // The last committed URL when the checker is constructed. It is used to
  // obtain page load token when the URL being checked is not a mainframe
  // URL.
  GURL last_committed_url_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform real time url check. Can only be
  // accessed in UI thread.
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui_;

  // May be null on certain platforms that don't support
  // chrome://safe-browsing and in unit tests. If non-null, guaranteed to
  // outlive this object by contract.
  raw_ptr<WebUIDelegate> webui_delegate_ = nullptr;

  // If the URL is classified as safe in cache manager during real time
  // lookup.
  bool is_cached_safe_url_ = false;

  // This will be created in cases where the real-time URL check decides to fall
  // back to the hash-based checks.
  std::unique_ptr<DatabaseManagerMechanism> hash_database_mechanism_ = nullptr;

  base::WeakPtrFactory<UrlRealTimeMechanism> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_REALTIME_MECHANISM_H_
