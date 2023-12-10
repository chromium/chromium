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
    base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
    MechanismExperimentHashDatabaseCache experiment_cache_selection,
    bool is_source_lookup_mechanism_experiment)
    : SafeBrowsingLookupMechanism(url,
                                  threat_types,
                                  database_manager,
                                  experiment_cache_selection),
      ui_task_runner_(ui_task_runner),
      lookup_service_on_ui_(lookup_service_on_ui),
      is_source_lookup_mechanism_experiment_(
          is_source_lookup_mechanism_experiment) {}

HashRealTimeMechanism::~HashRealTimeMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SafeBrowsingLookupMechanism::StartCheckResult
HashRealTimeMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  database_manager_->CheckUrlForHighConfidenceAllowlist(
      url_, "HPRT",
      base::BindOnce(
          &HashRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
          weak_factory_.GetWeakPtr()));

  return StartCheckResult(
      /*is_safe_synchronously=*/false);
}

void HashRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramEnumeration(
      "SafeBrowsing.HPRT.LocalMatch.Result",
      did_match_allowlist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH);

  if (did_match_allowlist) {
    // If the URL matches the high-confidence allowlist, still do the hash based
    // checks.
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/false);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HashRealTimeMechanism::StartLookupOnUIThread,
                       weak_factory_.GetWeakPtr(), url_,
                       is_source_lookup_mechanism_experiment_,
                       lookup_service_on_ui_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
  }
}

// static
void HashRealTimeMechanism::StartLookupOnUIThread(
    base::WeakPtr<HashRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    bool is_source_lookup_mechanism_experiment,
    base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  auto is_lookup_service_found = !!lookup_service_on_ui;
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.IsLookupServiceFound",
                            is_lookup_service_found);
  if (!is_lookup_service_found) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&HashRealTimeMechanism::PerformHashBasedCheck,
                       weak_ptr_on_io, url, /*real_time_request_failed=*/true));
    return;
  }

  HPRTLookupResponseCallback response_callback =
      base::BindOnce(&HashRealTimeMechanism::OnLookupResponse, weak_ptr_on_io);

  lookup_service_on_ui->StartLookup(url, is_source_lookup_mechanism_experiment,
                                    std::move(response_callback),
                                    std::move(io_task_runner));
}

void HashRealTimeMechanism::OnLookupResponse(
    bool is_lookup_successful,
    absl::optional<SBThreatType> threat_type,
    SBThreatType locally_cached_results_threat_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_lookup_successful) {
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/true);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
    return;
  }
  DCHECK(threat_type.has_value());
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type.value(), ThreatMetadata(),
      /*threat_source=*/ThreatSource::NATIVE_PVER5_REAL_TIME,
      /*url_real_time_lookup_response=*/nullptr,
      /*matched_high_confidence_allowlist=*/false,
      /*locally_cached_results_threat_type=*/locally_cached_results_threat_type,
      /*real_time_request_failed=*/!is_lookup_successful));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void HashRealTimeMechanism::PerformHashBasedCheck(
    const GURL& url,
    bool real_time_request_failed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hash_database_mechanism_ = std::make_unique<DatabaseManagerMechanism>(
      url, threat_types_, database_manager_, experiment_cache_selection_,
      CheckBrowseUrlType::kHashDatabase);
  auto result = hash_database_mechanism_->StartCheck(
      base::BindOnce(&HashRealTimeMechanism::OnHashDatabaseCompleteCheckResult,
                     weak_factory_.GetWeakPtr(), real_time_request_failed));
  if (result.is_safe_synchronously) {
    // No match found in the database, so conclude this is safe.
    OnHashDatabaseCompleteCheckResultInternal(
        SB_THREAT_TYPE_SAFE, ThreatMetadata(), /*threat_source=*/absl::nullopt,
        real_time_request_failed);
    // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
    // synchronous destruction of this object, so there is nothing safe to do
    // here but return.
  }
}

void HashRealTimeMechanism::OnHashDatabaseCompleteCheckResult(
    bool real_time_request_failed,
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  DCHECK(!result->real_time_request_failed);
  OnHashDatabaseCompleteCheckResultInternal(
      result->threat_type, result->metadata, result->threat_source,
      real_time_request_failed);
  // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
  // synchronous destruction of this object, so there is nothing safe to do here
  // but return.
}

void HashRealTimeMechanism::OnHashDatabaseCompleteCheckResultInternal(
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    absl::optional<ThreatSource> threat_source,
    bool real_time_request_failed) {
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type, metadata, threat_source,
      /*url_real_time_lookup_response=*/nullptr,
      /*matched_high_confidence_allowlist=*/!real_time_request_failed,
      /*locally_cached_results_threat_type=*/absl::nullopt,
      /*real_time_request_failed=*/real_time_request_failed));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

}  // namespace safe_browsing
