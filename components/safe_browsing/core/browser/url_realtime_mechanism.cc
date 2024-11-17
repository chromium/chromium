// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/common/features.h"

namespace safe_browsing {
namespace {

constexpr char kMatchResultHistogramName[] =
    "SafeBrowsing.RT.LocalMatch.Result";

void RecordLocalMatchResult(
    bool has_match,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details,
    std::string url_lookup_service_metric_suffix) {
  if (logging_details) {
    base::UmaHistogramBoolean("SafeBrowsing.RT.AllStoresAvailable",
                              logging_details->were_all_stores_available);
    base::UmaHistogramBoolean("SafeBrowsing.RT.AllowlistSizeTooSmall",
                              logging_details->was_allowlist_size_too_small);
  }

  AsyncMatch match_result =
      has_match ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH;
  base::UmaHistogramEnumeration(kMatchResultHistogramName, match_result);
  if (!url_lookup_service_metric_suffix.empty()) {
    base::UmaHistogramEnumeration(
        kMatchResultHistogramName + url_lookup_service_metric_suffix,
        match_result);
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class URTAndBackgroundHPRTResult {
  UrtSafeAndHprtUnfinished = 0,
  UrtUnsafeAndHprtUnfinished = 1,
  UrtSafeAndHprtSafe = 2,
  UrtSafeAndHprtUnsafe = 3,
  UrtUnsafeAndHprtSafe = 4,
  UrtUnsafeAndHprtUnsafe = 5,
  kMaxValue = UrtUnsafeAndHprtUnsafe
};

}  // namespace

UrlRealTimeMechanism::UrlRealTimeMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    SessionID tab_id,
    std::unique_ptr<SafeBrowsingLookupMechanism> hash_realtime_lookup_mechanism)
    : SafeBrowsingLookupMechanism(url, threat_types, database_manager),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      ui_task_runner_(ui_task_runner),
      url_lookup_service_on_ui_(url_lookup_service_on_ui),
      url_checker_delegate_(url_checker_delegate),
      web_contents_getter_(web_contents_getter),
      tab_id_(tab_id),
      hash_realtime_lookup_mechanism_(
          std::move(hash_realtime_lookup_mechanism)) {}

UrlRealTimeMechanism::~UrlRealTimeMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SafeBrowsingLookupMechanism::StartCheckResult
UrlRealTimeMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(url_lookup_service_metric_suffix_, kNoRealTimeURLLookupService);

  bool send_background_hprt_lookup = !!hash_realtime_lookup_mechanism_;
  if (send_background_hprt_lookup) {
    // Kick off hash realtime lookup.
    auto hprt_result = hash_realtime_lookup_mechanism_->StartCheck(
        base::BindOnce(&UrlRealTimeMechanism::OnHashRealTimeCompleteCheckResult,
                       weak_factory_.GetWeakPtr()));

    // If is_safe_synchronously value is true, we need to call the callback
    // function directly.
    if (hprt_result.is_safe_synchronously) {
      OnHashRealTimeCompleteCheckResultInternal(
          SBThreatType::SB_THREAT_TYPE_SAFE);
    }
  }

  bool check_allowlist = can_check_db_ && can_check_high_confidence_allowlist_;
  if (check_allowlist) {
    database_manager_->CheckUrlForHighConfidenceAllowlist(
        url_, base::BindOnce(
                  &UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
                  weak_factory_.GetWeakPtr()));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
            weak_factory_.GetWeakPtr(),
            /*url_on_high_confidence_allowlist=*/false,
            /*logging_details=*/std::nullopt));
  }
  base::UmaHistogramBoolean(
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      send_background_hprt_lookup);

  return StartCheckResult(
      /*is_safe_synchronously=*/false, /*threat_source=*/std::nullopt);
}

void UrlRealTimeMechanism::OnHashRealTimeCompleteCheckResult(
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnHashRealTimeCompleteCheckResultInternal(result->threat_type);
}

void UrlRealTimeMechanism::OnHashRealTimeCompleteCheckResultInternal(
    SBThreatType threat_type) {
  hash_realtime_lookup_result_threat_type_ = threat_type;
}

void UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordLocalMatchResult(did_match_allowlist, std::move(logging_details),
                         url_lookup_service_metric_suffix_);

  if (did_match_allowlist) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlRealTimeMechanism::MaybeSendSampleRequest,
                       weak_factory_.GetWeakPtr(), url_,
                       url_lookup_service_on_ui_, tab_id_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
    // If the URL matches the high-confidence allowlist, still do the hash based
    // checks.
    PerformHashBasedCheck(url_, HashDatabaseFallbackTrigger::kAllowlistMatch);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlRealTimeMechanism::StartLookupOnUIThread,
                       weak_factory_.GetWeakPtr(), url_,
                       url_lookup_service_on_ui_, tab_id_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
  }
}

// static
void UrlRealTimeMechanism::StartLookupOnUIThread(
    base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    SessionID tab_id,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  bool is_lookup_service_found = !!url_lookup_service_on_ui;
  base::UmaHistogramBoolean("SafeBrowsing.RT.IsLookupServiceFound",
                            is_lookup_service_found);
  if (!is_lookup_service_found) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlRealTimeMechanism::PerformHashBasedCheck,
                       weak_ptr_on_io, url,
                       HashDatabaseFallbackTrigger::kOriginalCheckFailed));
    return;
  }

  RTLookupResponseCallback response_callback =
      base::BindOnce(&UrlRealTimeMechanism::OnLookupResponse, weak_ptr_on_io);

  url_lookup_service_on_ui->StartLookup(url, std::move(response_callback),
                                        std::move(io_task_runner), tab_id);
}

void UrlRealTimeMechanism::MaybeSendSampleRequest(
    base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    SessionID tab_id,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  bool can_send_protego_sampled_ping =
      url_lookup_service_on_ui &&
      url_lookup_service_on_ui->CanSendRTSampleRequest();

  if (!can_send_protego_sampled_ping) {
    return;
  }
  bool is_lookup_service_available =
      !url_lookup_service_on_ui->IsInBackoffMode();
  if (is_lookup_service_available) {
    url_lookup_service_on_ui->SendSampledRequest(url, std::move(io_task_runner),
                                                 tab_id);
  }
}

void UrlRealTimeMechanism::OnLookupResponse(
    bool is_lookup_successful,
    bool is_cached_response,
    std::unique_ptr<RTLookupResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_lookup_successful) {
    PerformHashBasedCheck(url_,
                          HashDatabaseFallbackTrigger::kOriginalCheckFailed);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
    return;
  }

  RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type =
      RTLookupResponse::ThreatInfo::SAFE;
  SBThreatType sb_threat_type = SBThreatType::SB_THREAT_TYPE_SAFE;
  if (response && (response->threat_info_size() > 0)) {
    rt_verdict_type = response->threat_info(0).verdict_type();
    sb_threat_type =
        RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
            response->threat_info(0).threat_type(), rt_verdict_type);
  }

  MaybePerformSuspiciousSiteDetection(rt_verdict_type);

  if (is_cached_response &&
      sb_threat_type == SBThreatType::SB_THREAT_TYPE_SAFE) {
    is_cached_safe_url_ = true;
    PerformHashBasedCheck(url_, HashDatabaseFallbackTrigger::kCacheMatch);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  } else {
    CompleteCheckInternal(std::make_unique<CompleteCheckResult>(
        url_, sb_threat_type, ThreatMetadata(),
        ThreatSource::URL_REAL_TIME_CHECK, std::move(response)));
    // NOTE: Calling CompleteCheckInternal results in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  }
}

void UrlRealTimeMechanism::CompleteCheckInternal(
    std::unique_ptr<CompleteCheckResult> complete_check_result) {
  if (!!hash_realtime_lookup_mechanism_) {
    LogBackgroundHprtLookupResults(complete_check_result->threat_type);
  }

  // Call the CompleteCheck function to pass the final result to the callback.
  CompleteCheck(std::move(complete_check_result));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of
  // this object, so there is nothing safe to do here but return.
}

void UrlRealTimeMechanism::LogBackgroundHprtLookupResults(
    SBThreatType urt_threat_type) {
  // Compare the results from both the URL real-time lookup and the hash
  // real-time lookup. Return the URL real-time lookup result and send a CSBRR
  // if the URT and HPRT results differ.

  // Record metrics for background HPRT result.
  bool is_urt_safe = urt_threat_type == SBThreatType::SB_THREAT_TYPE_SAFE;
  if (!hash_realtime_lookup_result_threat_type_.has_value()) {
    if (is_urt_safe) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.URTAndBackgroundHPRT.Result",
          URTAndBackgroundHPRTResult::UrtSafeAndHprtUnfinished);
    } else {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.URTAndBackgroundHPRT.Result",
          URTAndBackgroundHPRTResult::UrtUnsafeAndHprtUnfinished);
    }
  } else {
    bool is_hprt_safe = hash_realtime_lookup_result_threat_type_.value() ==
                        SBThreatType::SB_THREAT_TYPE_SAFE;
    if (is_urt_safe && is_hprt_safe) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.URTAndBackgroundHPRT.Result",
          URTAndBackgroundHPRTResult::UrtSafeAndHprtSafe);
    } else if (is_urt_safe && !is_hprt_safe) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.URTAndBackgroundHPRT.Result",
          URTAndBackgroundHPRTResult::UrtSafeAndHprtUnsafe);
    } else if (!is_urt_safe && is_hprt_safe) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.URTAndBackgroundHPRT.Result",
          URTAndBackgroundHPRTResult::UrtUnsafeAndHprtSafe);
    } else if (!is_urt_safe && !is_hprt_safe) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.URTAndBackgroundHPRT.Result",
          URTAndBackgroundHPRTResult::UrtUnsafeAndHprtUnsafe);
    }
  }

  // We need to send a CSBRR if the URL real-time lookup verdict is different
  // from the HPRT lookup verdict.
  if (hash_realtime_lookup_result_threat_type_.has_value() &&
      urt_threat_type != hash_realtime_lookup_result_threat_type_.value()) {
    // Send new CSBRR report.
    auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
    report->set_type(ClientSafeBrowsingReportRequest::
                         URL_REALTIME_AND_HASH_REALTIME_DISCREPANCY);
    report->set_url(url_.spec());
    report->mutable_url_real_time_and_hash_real_time_discrepancy_info()
        ->set_url_realtime_threat_type(
            GetDiscrepancyThreatType(urt_threat_type));
    report->mutable_url_real_time_and_hash_real_time_discrepancy_info()
        ->set_hash_realtime_threat_type(GetDiscrepancyThreatType(
            hash_realtime_lookup_result_threat_type_.value()));

    url_checker_delegate_->SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
        std::move(report), web_contents_getter_);
  }
}

ClientSafeBrowsingReportRequest::UrlRealTimeAndHashRealTimeDiscrepancyInfo::
    LookupThreatType
    UrlRealTimeMechanism::GetDiscrepancyThreatType(SBThreatType threat_type) {
  switch (threat_type) {
    case SBThreatType::SB_THREAT_TYPE_URL_PHISHING:
      return ClientSafeBrowsingReportRequest::
          UrlRealTimeAndHashRealTimeDiscrepancyInfo::PHISHING;
    case SBThreatType::SB_THREAT_TYPE_URL_MALWARE:
      return ClientSafeBrowsingReportRequest::
          UrlRealTimeAndHashRealTimeDiscrepancyInfo::MALWARE;
    case SBThreatType::SB_THREAT_TYPE_URL_UNWANTED:
      return ClientSafeBrowsingReportRequest::
          UrlRealTimeAndHashRealTimeDiscrepancyInfo::UNWANTED;
    case SBThreatType::SB_THREAT_TYPE_BILLING:
      return ClientSafeBrowsingReportRequest::
          UrlRealTimeAndHashRealTimeDiscrepancyInfo::BILLING;
    default:
      return ClientSafeBrowsingReportRequest::
          UrlRealTimeAndHashRealTimeDiscrepancyInfo::SAFE_OR_OTHER;
  }
}

void UrlRealTimeMechanism::PerformHashBasedCheck(
    const GURL& url,
    HashDatabaseFallbackTrigger fallback_trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartCheckResult result(/*is_safe_synchronously=*/false, std::nullopt);
  if (can_check_db_) {
    hash_database_mechanism_ = std::make_unique<DatabaseManagerMechanism>(
        url, threat_types_, database_manager_,
        CheckBrowseUrlType::kHashDatabase,
        /*check_allowlist=*/false);
    result = hash_database_mechanism_->StartCheck(
        base::BindOnce(&UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResult,
                       weak_factory_.GetWeakPtr(), fallback_trigger));
  }
  if (result.is_safe_synchronously || !can_check_db_) {
    // No match found in the database, so conclude this is safe.
    OnHashDatabaseCompleteCheckResultInternal(
        SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata(),
        /*threat_source=*/result.threat_source, fallback_trigger);
    // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
    // synchronous destruction of this object, so there is nothing safe to do
    // here but return.
  }
}

void UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResult(
    HashDatabaseFallbackTrigger fallback_trigger,
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  OnHashDatabaseCompleteCheckResultInternal(
      result->threat_type, result->metadata, result->threat_source,
      fallback_trigger);
  // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
  // synchronous destruction of this object, so there is nothing safe to do here
  // but return.
}

void UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResultInternal(
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    std::optional<ThreatSource> threat_source,
    HashDatabaseFallbackTrigger fallback_trigger) {
  if (is_cached_safe_url_) {
    base::UmaHistogramEnumeration("SafeBrowsing.RT.GetCache.FallbackThreatType",
                                  threat_type);
  }
  LogHashDatabaseFallbackResult("RT", fallback_trigger, threat_type);
  CompleteCheckInternal(std::make_unique<CompleteCheckResult>(
      url_, threat_type, metadata, threat_source,
      /*url_real_time_lookup_response=*/nullptr));
  // NOTE: Calling CompleteCheckInternal results in the synchronous destruction
  // of this object, so there is nothing safe to do here but return.
}

void UrlRealTimeMechanism::MaybePerformSuspiciousSiteDetection(
    RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type) {
  if (rt_verdict_type == RTLookupResponse::ThreatInfo::SUSPICIOUS) {
    url_checker_delegate_->NotifySuspiciousSiteDetected(web_contents_getter_);
  }
}

}  // namespace safe_browsing
