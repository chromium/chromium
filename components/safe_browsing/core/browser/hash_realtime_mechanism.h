// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_REALTIME_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_REALTIME_MECHANISM_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/database_manager_mechanism.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "url/gurl.h"

namespace safe_browsing {

// This performs the hash-prefix real-time Safe Browsing check.
class HashRealTimeMechanism : public SafeBrowsingLookupMechanism {
 public:
  HashRealTimeMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<HashRealTimeService> lookup_service_on_ui);
  HashRealTimeMechanism(const HashRealTimeMechanism&) = delete;
  HashRealTimeMechanism& operator=(const HashRealTimeMechanism&) = delete;
  ~HashRealTimeMechanism() override;

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

  // This function has to be static because it is called in UI thread. This
  // function starts a hash-prefix real-time lookup if |lookup_service_on_ui| is
  // available and is not in backoff mode. Otherwise, it hops back to the IO
  // thread and performs the hash-based database check.
  static void StartLookupOnUIThread(
      base::WeakPtr<HashRealTimeMechanism> weak_checker_on_io,
      const GURL& url,
      base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Called when the |response| from the real-time lookup service is received.
  // |is_lookup_successful| is true if the response code is OK and the
  // response body is successfully parsed.
  // |threat_type| will not be populated if the lookup was unsuccessful, but
  // will otherwise always be populated with the result of the lookup.
  void OnLookupResponse(bool is_lookup_successful,
                        std::optional<SBThreatType> threat_type);

  // Perform the hash-based database check for the url.
  void PerformHashBasedCheck(const GURL& url,
                             HashDatabaseFallbackTrigger fallback_trigger);

  // The hash-prefix real-time check can sometimes default back to the
  // hash-based database check. In these cases, this function is called once the
  // check has completed, which reports back the final results to the caller.
  void OnHashDatabaseCompleteCheckResult(
      HashDatabaseFallbackTrigger fallback_trigger,
      std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result);
  void OnHashDatabaseCompleteCheckResultInternal(
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      std::optional<ThreatSource> threat_source,
      HashDatabaseFallbackTrigger fallback_trigger);

  SEQUENCE_CHECKER(sequence_checker_);

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform the hash-prefix real-time check. Can only be
  // accessed in UI thread.
  base::WeakPtr<HashRealTimeService> lookup_service_on_ui_;

  // This will be created in cases where the hash-prefix real-time check decides
  // to fall back to the hash-based database checks.
  std::unique_ptr<DatabaseManagerMechanism> hash_database_mechanism_ = nullptr;

  base::WeakPtrFactory<HashRealTimeMechanism> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_REALTIME_MECHANISM_H_
