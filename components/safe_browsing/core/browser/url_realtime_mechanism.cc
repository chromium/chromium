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
  base::UmaHistogramEnumeration(kMatchResultHistogramName + frame_suffix +
                                    url_lookup_service_metric_suffix,
                                match_result);
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
    MechanismExperimentHashDatabaseCache experiment_cache_selection)
    : SafeBrowsingLookupMechanism(url,
                                  threat_types,
                                  database_manager,
                                  can_check_db,
                                  experiment_cache_selection),
      request_destination_(request_destination),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      last_committed_url_(last_committed_url),
      ui_task_runner_(ui_task_runner),
      url_lookup_service_on_ui_(url_lookup_service_on_ui),
      webui_delegate_(webui_delegate) {}

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
  bool has_allowlist_match =
      check_allowlist &&
      database_manager_->CheckUrlForHighConfidenceAllowlist(url_, "RT");
  RecordLocalMatchResult(has_allowlist_match, request_destination_,
                         url_lookup_service_metric_suffix_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
          weak_factory_.GetWeakPtr(),
          /*did_match_allowlist=*/has_allowlist_match));

  return StartCheckResult(
      /*is_safe_synchronously=*/false,
      /*did_check_url_real_time_allowlist=*/check_allowlist);
}

void UrlRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
    PerformHashBasedCheck(url_);
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
        FROM_HERE, base::BindOnce(&UrlRealTimeMechanism::PerformHashBasedCheck,
                                  weak_ptr_on_io, url));
    return;
  }

  RTLookupRequestCallback request_callback =
      base::BindOnce(&UrlRealTimeMechanism::OnRTLookupRequest, weak_ptr_on_io);

  RTLookupResponseCallback response_callback =
      base::BindOnce(&UrlRealTimeMechanism::OnRTLookupResponse, weak_ptr_on_io);

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
    RTLookupRequestCallback request_callback = base::BindOnce(
        &UrlRealTimeMechanism::OnRTLookupRequest, weak_ptr_on_io);
    url_lookup_service_on_ui->SendSampledRequest(
        url, last_committed_url, is_mainframe, std::move(request_callback),
        std::move(io_task_runner));
  }
}

void UrlRealTimeMechanism::OnRTLookupRequest(
    std::unique_ptr<RTLookupRequest> request,
    std::string oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LogRTLookupRequest(*request, oauth_token);
}

void UrlRealTimeMechanism::OnRTLookupResponse(
    bool is_rt_lookup_successful,
    bool is_cached_response,
    std::unique_ptr<RTLookupResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_rt_lookup_successful) {
    PerformHashBasedCheck(url_);
    return;
  }

  LogRTLookupResponse(*response);

  // Filter the response to remove enterprise verdicts if experiment is not
  // enabled for Managed Policy UrlFiltering
  if (!base::FeatureList::IsEnabled((kRealTimeUrlFilteringForEnterprise))) {
    auto* response_threat_info = response->mutable_threat_info();
    auto unsupported = std::remove_if(
        response_threat_info->begin(), response_threat_info->end(),
        [](const auto& threat_info) {
          return threat_info.threat_type() ==
                 RTLookupResponse::ThreatInfo::MANAGED_POLICY;
        });
    response_threat_info->erase(unsupported, response_threat_info->end());
  }

  SBThreatType sb_threat_type = SB_THREAT_TYPE_SAFE;
  if (response && (response->threat_info_size() > 0)) {
    sb_threat_type =
        RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
            response->threat_info(0).threat_type(),
            response->threat_info(0).verdict_type());
  }

  if (is_cached_response && sb_threat_type == SB_THREAT_TYPE_SAFE) {
    is_cached_safe_url_ = true;
    PerformHashBasedCheck(url_);
  } else {
    CompleteCheck(std::make_unique<CompleteCheckResult>(
        url_, sb_threat_type, ThreatMetadata(),
        /*is_from_url_real_time_check=*/true, std::move(response)));
  }
}

void UrlRealTimeMechanism::LogRTLookupRequest(const RTLookupRequest& request,
                                              const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!webui_delegate_) {
    return;
  }

  // The following is to log this RTLookupRequest on any open
  // chrome://safe-browsing pages.
  ui_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WebUIDelegate::AddToRTLookupPings,
                     base::Unretained(webui_delegate_), request, oauth_token),
      base::BindOnce(&UrlRealTimeMechanism::SetWebUIToken,
                     weak_factory_.GetWeakPtr()));
}

void UrlRealTimeMechanism::LogRTLookupResponse(
    const RTLookupResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!webui_delegate_) {
    return;
  }

  if (url_web_ui_token_ != -1) {
    // The following is to log this RTLookupResponse on any open
    // chrome://safe-browsing pages.
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebUIDelegate::AddToRTLookupResponses,
                                  base::Unretained(webui_delegate_),
                                  url_web_ui_token_, response));
  }
}

void UrlRealTimeMechanism::SetWebUIToken(int token) {
  url_web_ui_token_ = token;
}

void UrlRealTimeMechanism::PerformHashBasedCheck(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hash_database_mechanism_ = std::make_unique<HashDatabaseMechanism>(
      url, threat_types_, database_manager_, can_check_db_,
      experiment_cache_selection_);
  auto result = hash_database_mechanism_->StartCheck(
      base::BindOnce(&UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResult,
                     weak_factory_.GetWeakPtr()));
  if (result.is_safe_synchronously) {
    // No match found in the database, so conclude this is safe.
    OnHashDatabaseCompleteCheckResultInternal(SB_THREAT_TYPE_SAFE,
                                              ThreatMetadata());
  }
}

void UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResult(
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  OnHashDatabaseCompleteCheckResultInternal(result->threat_type,
                                            result->metadata);
}

void UrlRealTimeMechanism::OnHashDatabaseCompleteCheckResultInternal(
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  if (is_cached_safe_url_) {
    UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.GetCache.FallbackThreatType",
                              threat_type, SB_THREAT_TYPE_MAX + 1);
  }
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type, metadata,
      /*is_from_url_real_time_check=*/false,
      /*url_real_time_lookup_response=*/nullptr));
}

}  // namespace safe_browsing
