// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/common/utils.h"

namespace safe_browsing {

HashRealTimeMechanism::HashRealTimeMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<HashRealTimeService> lookup_service_on_ui)
    : SafeBrowsingLookupMechanism(url, threat_types, database_manager),
      ui_task_runner_(ui_task_runner),
      lookup_service_on_ui_(lookup_service_on_ui) {}

HashRealTimeMechanism::~HashRealTimeMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SafeBrowsingLookupMechanism::StartCheckResult
HashRealTimeMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_, base::BindOnce(
                &HashRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
                weak_factory_.GetWeakPtr()));

  return StartCheckResult(
      /*is_safe_synchronously=*/false, /*threat_source=*/std::nullopt);
}

void HashRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramEnumeration(
      "SafeBrowsing.HPRT.LocalMatch.Result",
      did_match_allowlist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH);

  if (logging_details) {
    base::UmaHistogramBoolean("SafeBrowsing.HPRT.AllStoresAvailable",
                              logging_details->were_all_stores_available);
    base::UmaHistogramBoolean("SafeBrowsing.HPRT.AllowlistSizeTooSmall",
                              logging_details->was_allowlist_size_too_small);
  }

  if (did_match_allowlist) {
    // If the URL matches the high-confidence allowlist, still do the hash based
    // checks.
    PerformHashBasedCheck(url_, HashDatabaseFallbackTrigger::kAllowlistMatch);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HashRealTimeMechanism::StartLookupOnUIThread,
                       weak_factory_.GetWeakPtr(), url_,
                       lookup_service_on_ui_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
  }
}

// static
void HashRealTimeMechanism::StartLookupOnUIThread(
    base::WeakPtr<HashRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  auto is_lookup_service_found = !!lookup_service_on_ui;
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.IsLookupServiceFound",
                            is_lookup_service_found);
  if (!is_lookup_service_found) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&HashRealTimeMechanism::PerformHashBasedCheck,
                       weak_ptr_on_io, url,
                       HashDatabaseFallbackTrigger::kOriginalCheckFailed));
    return;
  }

  HPRTLookupResponseCallback response_callback =
      base::BindOnce(&HashRealTimeMechanism::OnLookupResponse, weak_ptr_on_io);

  lookup_service_on_ui->StartLookup(url, std::move(response_callback),
                                    std::move(io_task_runner));
}

void HashRealTimeMechanism::OnLookupResponse(
    bool is_lookup_successful,
    std::optional<SBThreatType> threat_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_lookup_successful) {
    PerformHashBasedCheck(url_,
                          HashDatabaseFallbackTrigger::kOriginalCheckFailed);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
    return;
  }
  DCHECK(threat_type.has_value());
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type.value(), ThreatMetadata(),
      /*threat_source=*/ThreatSource::NATIVE_PVER5_REAL_TIME,
      /*url_real_time_lookup_response=*/nullptr));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void HashRealTimeMechanism::PerformHashBasedCheck(
    const GURL& url,
    HashDatabaseFallbackTrigger fallback_trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hash_database_mechanism_ = std::make_unique<DatabaseManagerMechanism>(
      url, threat_types_, database_manager_, CheckBrowseUrlType::kHashDatabase,
      /*check_allowlist=*/false);
  auto result = hash_database_mechanism_->StartCheck(
      base::BindOnce(&HashRealTimeMechanism::OnHashDatabaseCompleteCheckResult,
                     weak_factory_.GetWeakPtr(), fallback_trigger));
  if (result.is_safe_synchronously) {
    // No match found in the database, so conclude this is safe.
    OnHashDatabaseCompleteCheckResultInternal(
        SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata(),
        /*threat_source=*/result.threat_source, fallback_trigger);
    // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
    // synchronous destruction of this object, so there is nothing safe to do
    // here but return.
  }
}

void HashRealTimeMechanism::OnHashDatabaseCompleteCheckResult(
    HashDatabaseFallbackTrigger fallback_trigger,
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  OnHashDatabaseCompleteCheckResultInternal(
      result->threat_type, result->metadata, result->threat_source,
      fallback_trigger);
  // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
  // synchronous destruction of this object, so there is nothing safe to do here
  // but return.
}

void HashRealTimeMechanism::OnHashDatabaseCompleteCheckResultInternal(
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    std::optional<ThreatSource> threat_source,
    HashDatabaseFallbackTrigger fallback_trigger) {
  CHECK(fallback_trigger == HashDatabaseFallbackTrigger::kAllowlistMatch ||
        fallback_trigger == HashDatabaseFallbackTrigger::kOriginalCheckFailed);
  LogHashDatabaseFallbackResult("HPRT", fallback_trigger, threat_type);
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type, metadata, threat_source,
      /*url_real_time_lookup_response=*/nullptr));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

}  // namespace safe_browsing
