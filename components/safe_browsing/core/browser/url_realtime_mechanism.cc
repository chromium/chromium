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
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/common/features.h"

namespace safe_browsing {
namespace {

constexpr char kMatchResultHistogramName[] =
    "SafeBrowsing.RT.LocalMatch.Result";

void RecordLocalMatchResult(
    bool has_match,
    network::mojom::RequestDestination request_destination,
    std::string url_lookup_service_metric_suffix) {
  AsyncMatch match_result =
      has_match ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH;
  base::UmaHistogramEnumeration(kMatchResultHistogramName, match_result);
  bool is_mainframe =
      request_destination == network::mojom::RequestDestination::kDocument;
  std::string frame_suffix = is_mainframe ? ".Mainframe" : ".NonMainframe";
  base::UmaHistogramEnumeration(kMatchResultHistogramName + frame_suffix,
                                match_result);
  if (!url_lookup_service_metric_suffix.empty()) {
    base::UmaHistogramEnumeration(kMatchResultHistogramName + frame_suffix +
                                      url_lookup_service_metric_suffix,
                                  match_result);
  }
}

}  // namespace

UrlRealTimeMechanism::UrlRealTimeMechanism(
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
    MechanismExperimentHashDatabaseCache experiment_cache_selection,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter)
    : SafeBrowsingLookupMechanism(url,
                                  threat_types,
                                  database_manager,
                                  experiment_cache_selection),
      request_destination_(request_destination),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      last_committed_url_(last_committed_url),
      ui_task_runner_(ui_task_runner),
      url_lookup_service_on_ui_(url_lookup_service_on_ui),
      webui_delegate_(webui_delegate),
      url_checker_delegate_(url_checker_delegate),
      web_contents_getter_(web_contents_getter) {}

UrlRealTimeMechanism::~UrlRealTimeMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SafeBrowsingLookupMechanism::StartCheckResult
UrlRealTimeMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(url_lookup_service_metric_suffix_, kNoRealTimeURLLookupService);
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.RequestDestinations.Checked",
                            request_destination_);

  bool check_allowlist = can_check_db_ && can_check_high_confidence_allowlist_;
  if (check_allowlist) {
    database_manager_->CheckUrlForHighConfidenceAllowlist(
        url_, "RT",
        base::BindOnce(
            &UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
            weak_factory_.GetWeakPtr()));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
            weak_factory_.GetWeakPtr(), /*did_match_allowlist=*/false));
  }

  return StartCheckResult(
      /*is_safe_synchronously=*/false);
}

void UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  did_match_allowlist_ = did_match_allowlist;

  RecordLocalMatchResult(did_match_allowlist, request_destination_,
                         url_lookup_service_metric_suffix_);

  if (did_match_allowlist) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlRealTimeMechanism::MaybeSendSampleRequest,
                       weak_factory_.GetWeakPtr(), url_, last_committed_url_,
                       /*is_mainframe=*/request_destination_ ==
                           network::mojom::RequestDestination::kDocument,
                       url_lookup_service_on_ui_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
    // If the URL matches the high-confidence allowlist, still do the hash based
    // checks.
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/false);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlRealTimeMechanism::StartLookupOnUIThread,
                       weak_factory_.GetWeakPtr(), url_, last_committed_url_,
                       /*is_mainframe=*/request_destination_ ==
                           network::mojom::RequestDestination::kDocument,
                       url_lookup_service_on_ui_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
  }
}

// static
void UrlRealTimeMechanism::StartLookupOnUIThread(
    base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  bool is_lookup_service_available =
      url_lookup_service_on_ui && !url_lookup_service_on_ui->IsInBackoffMode();
  base::UmaHistogramBoolean("SafeBrowsing.RT.IsLookupServiceAvailable",
                            is_lookup_service_available);
  if (!is_lookup_service_available) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlRealTimeMechanism::PerformHashBasedCheck,
                       weak_ptr_on_io, url, /*real_time_request_failed=*/true));
    return;
  }

  RTLookupRequestCallback request_callback =
      base::BindOnce(&UrlRealTimeMechanism::OnLookupRequest, weak_ptr_on_io);

  RTLookupResponseCallback response_callback =
      base::BindOnce(&UrlRealTimeMechanism::OnLookupResponse, weak_ptr_on_io);

  url_lookup_service_on_ui->StartLookup(
      url, last_committed_url, is_mainframe, std::move(request_callback),
      std::move(response_callback), std::move(io_task_runner));
}

void UrlRealTimeMechanism::MaybeSendSampleRequest(
    base::WeakPtr<UrlRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
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
    RTLookupRequestCallback request_callback =
        base::BindOnce(&UrlRealTimeMechanism::OnLookupRequest, weak_ptr_on_io);
    url_lookup_service_on_ui->SendSampledRequest(
        url, last_committed_url, is_mainframe, std::move(request_callback),
        std::move(io_task_runner));
  }
}

void UrlRealTimeMechanism::OnLookupRequest(
    std::unique_ptr<RTLookupRequest> request,
    std::string oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LogLookupRequest(*request, oauth_token);
}

void UrlRealTimeMechanism::OnLookupResponse(
    bool is_lookup_successful,
    bool is_cached_response,
    std::unique_ptr<RTLookupResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_lookup_successful) {
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/true);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
    return;
  }

  LogLookupResponse(*response);

  RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type =
      RTLookupResponse::ThreatInfo::SAFE;
  SBThreatType sb_threat_type = SB_THREAT_TYPE_SAFE;
  if (response && (response->threat_info_size() > 0)) {
    rt_verdict_type = response->threat_info(0).verdict_type();
    sb_threat_type =
        RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
            response->threat_info(0).threat_type(), rt_verdict_type);
  }

  MaybePerformSuspiciousSiteDetection(rt_verdict_type);

  if (is_cached_response && sb_threat_type == SB_THREAT_TYPE_SAFE) {
    is_cached_safe_url_ = true;
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/false);
    // NOTE: Calling PerformHashBasedCheck may result in the synchronous
    // destruction of this object, so there is nothing safe to do here but
    // return.
  } else {
    CompleteCheck(std::make_unique<CompleteCheckResult>(
        url_, sb_threat_type, ThreatMetadata(),
        ThreatSource::URL_REAL_TIME_CHECK, std::move(response),
        /*matched_high_confidence_allowlist=*/did_match_allowlist_,
        /*locally_cached_results_threat_type=*/
        is_cached_response ? sb_threat_type : SBThreatType::SB_THREAT_TYPE_SAFE,
        /*real_time_request_failed=*/false));
    // NOTE: Calling CompleteCheck results in the synchronous destruction of
    // this object, so there is nothing safe to do here but return.
  }
}

void UrlRealTimeMechanism::LogLookupRequest(const RTLookupRequest& request,
                                            const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!webui_delegate_) {
    return;
  }

  // The following is to log this lookup request on any open
  // chrome://safe-browsing pages.
  ui_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WebUIDelegate::AddToURTLookupPings,
                     base::Unretained(webui_delegate_), request, oauth_token),
      base::BindOnce(&UrlRealTimeMechanism::SetWebUIToken,
                     weak_factory_.GetWeakPtr()));
}

void UrlRealTimeMechanism::LogLookupResponse(const RTLookupResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!webui_delegate_) {
    return;
  }

  if (url_web_ui_token_ != -1) {
    // The following is to log this lookup response on any open
    // chrome://safe-browsing pages.
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebUIDelegate::AddToURTLookupResponses,
                                  base::Unretained(webui_delegate_),
                                  url_web_ui_token_, response));
  }
}

void UrlRealTimeMechanism::SetWebUIToken(int token) {
  url_web_ui_token_ = token;
}

void UrlRealTimeMechanism::PerformHashBasedCheck(
    const GURL& url,
    bool real_time_request_failed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_safe_synchronously = false;
  if (can_check_db_) {
    hash_database_mechanism_ = std::make_unique<DatabaseManagerMechanism>(
        url, threat_types_, database_manager_, experiment_cache_selection_,
        CheckBrowseUrlType::kHashDatabase);
    is_safe_synchronously =
        hash_database_mechanism_
            ->StartCheck(base::BindOnce(
                &UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResult,
                weak_factory_.GetWeakPtr(), real_time_request_failed))
            .is_safe_synchronously;
  }
  if (is_safe_synchronously || !can_check_db_) {
    // No match found in the database, so conclude this is safe.
    OnHashDatabaseCompleteCheckResultInternal(
        SB_THREAT_TYPE_SAFE, ThreatMetadata(), /*threat_source=*/absl::nullopt,
        real_time_request_failed);
    // NOTE: Calling OnHashDatabaseCompleteCheckResultInternal results in the
    // synchronous destruction of this object, so there is nothing safe to do
    // here but return.
  }
}

void UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResult(
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

void UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResultInternal(
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    absl::optional<ThreatSource> threat_source,
    bool real_time_request_failed) {
  if (is_cached_safe_url_) {
    UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.GetCache.FallbackThreatType",
                              threat_type, SB_THREAT_TYPE_MAX + 1);
  }
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type, metadata, threat_source,
      /*url_real_time_lookup_response=*/nullptr,
      /*matched_high_confidence_allowlist=*/did_match_allowlist_,
      /*locally_cached_results_threat_type=*/
      is_cached_safe_url_
          ? absl::optional<SBThreatType>(SBThreatType::SB_THREAT_TYPE_SAFE)
          : absl::nullopt,
      /*real_time_request_failed=*/real_time_request_failed));
  // NOTE: Calling CompleteCheck results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void UrlRealTimeMechanism::MaybePerformSuspiciousSiteDetection(
    RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type) {
  if (rt_verdict_type == RTLookupResponse::ThreatInfo::SUSPICIOUS &&
      base::FeatureList::IsEnabled(
          safe_browsing::kSuspiciousSiteDetectionRTLookups)) {
    url_checker_delegate_->NotifySuspiciousSiteDetected(web_contents_getter_);
  }
}

}  // namespace safe_browsing
