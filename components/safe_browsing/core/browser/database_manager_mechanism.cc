// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/database_manager_mechanism.h"

#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"

namespace safe_browsing {

DatabaseManagerMechanism::DatabaseManagerMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    CheckBrowseUrlType check_type,
    bool check_allowlist,
    base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
        v5_get_hash_protocol_manager)
    : SafeBrowsingLookupMechanism(url, threat_types, database_manager),
      SafeBrowsingDatabaseManager::Client(GetPassKey()),
      check_allowlist_(check_allowlist),
      check_type_(check_type),
      v5_get_hash_protocol_manager_(v5_get_hash_protocol_manager) {}

DatabaseManagerMechanism::~DatabaseManagerMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_async_blocklist_check_in_progress_) {
    database_manager_->CancelCheck(this);
  }
}

ThreatSource DatabaseManagerMechanism::GetThreatSource() const {
  return database_manager_->GetBrowseUrlThreatSource(check_type_);
}

SafeBrowsingLookupMechanism::StartCheckResult
DatabaseManagerMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!check_allowlist_) {
    return StartBlocklistCheck();
  }

  database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_, base::BindOnce(
                &DatabaseManagerMechanism::OnCheckUrlForHighConfidenceAllowlist,
                weak_factory_.GetWeakPtr()));
  return StartCheckResult(
      /*is_safe_synchronously=*/false, /*threat_source=*/std::nullopt);
}

void DatabaseManagerMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(check_allowlist_);

  if (did_match_allowlist) {
    CompleteCheck(std::make_unique<CompleteCheckResult>(
        url_, SBThreatType::SB_THREAT_TYPE_SAFE,
        /*threat_source=*/std::nullopt,
        /*url_real_time_lookup_response=*/nullptr));
    // NOTE: Calling CompleteCheck results in the synchronous destruction of
    // this object, so there is nothing safe to do here but return.
    return;
  } else {
    StartBlocklistCheckAfterAllowlistCheck();
  }
}

SafeBrowsingLookupMechanism::StartCheckResult
DatabaseManagerMechanism::StartBlocklistCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_safe_synchronously =
      database_manager_->CheckBrowseUrl(url_, threat_types_, this, check_type_);
  if (is_safe_synchronously) {
    LogCheckResult(SBThreatType::SB_THREAT_TYPE_SAFE);
  } else {
    is_async_blocklist_check_in_progress_ = true;
  }
  return StartCheckResult(is_safe_synchronously, GetThreatSource());
}

void DatabaseManagerMechanism::StartBlocklistCheckAfterAllowlistCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_safe_synchronously =
      database_manager_->CheckBrowseUrl(url_, threat_types_, this, check_type_);
  if (is_safe_synchronously) {
    LogCheckResult(SBThreatType::SB_THREAT_TYPE_SAFE);
    CompleteCheck(std::make_unique<CompleteCheckResult>(
        url_, SBThreatType::SB_THREAT_TYPE_SAFE,
        /*threat_source=*/std::nullopt,
        /*url_real_time_lookup_response=*/nullptr));
    // NOTE: Calling CompleteCheck results in the synchronous destruction of
    // this object, so there is nothing safe to do here but return.
    return;
  } else {
    is_async_blocklist_check_in_progress_ = true;
  }
}

void DatabaseManagerMechanism::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_async_blocklist_check_in_progress_ = false;
  LogCheckResult(threat_type);
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url, threat_type, GetThreatSource(),
      /*url_real_time_lookup_response=*/nullptr));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void DatabaseManagerMechanism::LogCheckResult(SBThreatType threat_type) {
  base::UmaHistogramEnumeration("SafeBrowsing.CheckUrl.Result.HashDatabase",
                                threat_type);
}

base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
DatabaseManagerMechanism::GetV5GetHashProtocolManager() {
  return v5_get_hash_protocol_manager_;
}

}  // namespace safe_browsing
