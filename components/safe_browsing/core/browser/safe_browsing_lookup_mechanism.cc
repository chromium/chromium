// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"

namespace safe_browsing {

SafeBrowsingLookupMechanism::SafeBrowsingLookupMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool can_check_db,
    MechanismExperimentHashDatabaseCache experiment_cache_selection)
    : url_(url),
      threat_types_(threat_types),
      database_manager_(database_manager),
      can_check_db_(can_check_db),
      experiment_cache_selection_(experiment_cache_selection) {}

SafeBrowsingLookupMechanism::~SafeBrowsingLookupMechanism() = default;

SafeBrowsingLookupMechanism::StartCheckResult::StartCheckResult(
    bool is_safe_synchronously,
    bool did_check_url_real_time_allowlist)
    : is_safe_synchronously(is_safe_synchronously),
      did_check_url_real_time_allowlist(did_check_url_real_time_allowlist) {}

SafeBrowsingLookupMechanism::CompleteCheckResult::CompleteCheckResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    bool is_from_url_real_time_check,
    std::unique_ptr<RTLookupResponse> url_real_time_lookup_response)
    : url(url),
      threat_type(threat_type),
      metadata(metadata),
      is_from_url_real_time_check(is_from_url_real_time_check),
      url_real_time_lookup_response(std::move(url_real_time_lookup_response)) {}

SafeBrowsingLookupMechanism::CompleteCheckResult::~CompleteCheckResult() =
    default;

SafeBrowsingLookupMechanism::StartCheckResult
SafeBrowsingLookupMechanism::StartCheck(
    CompleteCheckResultCallback complete_check_callback) {
#if DCHECK_IS_ON()
  DCHECK(!has_started_check_);
  has_started_check_ = true;
#endif
  complete_check_callback_ = std::move(complete_check_callback);
  return StartCheckInternal();
}

void SafeBrowsingLookupMechanism::CompleteCheck(
    std::unique_ptr<CompleteCheckResult> result) {
  DCHECK(complete_check_callback_);
  std::move(complete_check_callback_).Run(std::move(result));
}

}  // namespace safe_browsing
