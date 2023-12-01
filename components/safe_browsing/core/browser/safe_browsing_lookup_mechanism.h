// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_H_

#include "base/functional/callback_forward.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

// This is the base class for specific lookup mechanism objects and holds the
// shared logic among the objects. Each lookup mechanism object is responsible
// for starting the lookup mechanism and eventually returning the results up to
// the caller.
class SafeBrowsingLookupMechanism {
 public:
  struct StartCheckResult {
    explicit StartCheckResult(bool is_safe_synchronously);
    bool is_safe_synchronously;
  };

  // This is used by individual lookup mechanisms as the input for the
  // |complete_check_callback_| to the consumer. It contains the details about
  // the mechanism's lookup.
  struct CompleteCheckResult {
    CompleteCheckResult(
        const GURL& url,
        SBThreatType threat_type,
        const ThreatMetadata& metadata,
        absl::optional<ThreatSource> threat_source,
        std::unique_ptr<RTLookupResponse> url_real_time_lookup_response,
        absl::optional<bool> matched_high_confidence_allowlist,
        absl::optional<SBThreatType> locally_cached_results_threat_type,
        bool real_time_request_failed);
    ~CompleteCheckResult();
    GURL url;
    SBThreatType threat_type;
    ThreatMetadata metadata;
    // Specifies the threat source associated with the mechanism that provided
    // the threat type. In cases where a real-time mechanism falls back to the
    // hash database mechanism, the threat source will correspond to the hash
    // database mechanism. This value only guaranteed to be non-null in cases
    // where the threat type is not SB_THREAT_TYPE_SAFE; in cases where the hash
    // database mechanism fallback completes synchronously, this is unset.
    absl::optional<ThreatSource> threat_source;
    std::unique_ptr<RTLookupResponse> url_real_time_lookup_response;

    // TODO(crbug.com/1410253): Deprecate these once the experiment is complete.
    // This can be absl::nullopt if the allowlist check is irrelevant to the
    // mechanism.
    absl::optional<bool> matched_high_confidence_allowlist;
    absl::optional<SBThreatType> locally_cached_results_threat_type;
    bool real_time_request_failed;
  };
  using CompleteCheckResultCallback =
      base::OnceCallback<void(std::unique_ptr<CompleteCheckResult> result)>;

  SafeBrowsingLookupMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      MechanismExperimentHashDatabaseCache experiment_cache_selection);
  virtual ~SafeBrowsingLookupMechanism();
  SafeBrowsingLookupMechanism(const SafeBrowsingLookupMechanism&) = delete;
  SafeBrowsingLookupMechanism& operator=(const SafeBrowsingLookupMechanism&) =
      delete;

  // Starts the lookup mechanism check. This should only be called once. If the
  // check completes synchronously, then it will specify |is_safe_synchronously|
  // in the result instead of calling the |complete_check_callback| callback.
  StartCheckResult StartCheck(
      CompleteCheckResultCallback complete_check_callback);

 protected:
  // Called by children classes when the check is complete (unless it completes
  // synchronously). This should only be called once, since it calls std::move
  // on |complete_check_callback_|.
  void CompleteCheck(std::unique_ptr<CompleteCheckResult> result);

  // The URL to run the lookup for.
  GURL url_;

  // Which threat types are eligible for the check.
  SBThreatTypeSet threat_types_;

  // Used for interactions with the database, such as running a hash-based
  // check or checking the high-confidence allowlist.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Specifies which hash database cache to use if a hash-database lookup ends
  // up occurring. For more details, see the comments above the definition of
  // MechanismExperimentHashDatabaseCache.
  MechanismExperimentHashDatabaseCache experiment_cache_selection_;

 private:
  // |StartCheck| has some logic used across mechanisms. |StartCheckInternal| is
  // implemented only by the inheriting classes and performs the mechanism-
  // specific logic for starting the check.
  virtual StartCheckResult StartCheckInternal() = 0;

  // The callback that will eventually be called when the check completes,
  // unless it completes synchronously.
  CompleteCheckResultCallback complete_check_callback_;

#if DCHECK_IS_ON()
  // Used only to validate that StartCheck is called only once.
  bool has_started_check_ = false;
#endif
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_H_
