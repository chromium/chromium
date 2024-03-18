// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

namespace safe_browsing {

SafeBrowsingLookupMechanism::SafeBrowsingLookupMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : url_(url),
      threat_types_(threat_types),
      database_manager_(database_manager) {}

SafeBrowsingLookupMechanism::~SafeBrowsingLookupMechanism() = default;

SafeBrowsingLookupMechanism::StartCheckResult::StartCheckResult(
    bool is_safe_synchronously)
    : is_safe_synchronously(is_safe_synchronously) {}

SafeBrowsingLookupMechanism::CompleteCheckResult::CompleteCheckResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    std::optional<ThreatSource> threat_source,
    std::unique_ptr<RTLookupResponse> url_real_time_lookup_response)
    : url(url),
      threat_type(threat_type),
      metadata(metadata),
      threat_source(threat_source),
      url_real_time_lookup_response(std::move(url_real_time_lookup_response)) {
  DCHECK(threat_source.has_value() ||
         threat_type == SBThreatType::SB_THREAT_TYPE_SAFE);
}

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
  // NOTE: Invoking the callback results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

}  // namespace safe_browsing
