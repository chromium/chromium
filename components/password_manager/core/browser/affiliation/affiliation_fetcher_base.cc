// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_base.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/browser/affiliation/lookup_affiliation_response_parser.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace password_manager {

// Enumeration listing the possible outcomes of fetching affiliation information
// from the Affiliation API. This is used in UMA histograms, so do not change
// existing values, only add new values at the end.
enum class AffiliationFetchResult {
  kSuccess = 0,
  kFailure = 1,
  kMalformed = 2,
  kMaxValue = kMalformed,
};

namespace {

void LogFetchResult(AffiliationFetchResult result,
                    base::TimeDelta fetch_time,
                    size_t response_size = 0) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AffiliationFetcher.FetchResult", result);

  switch (result) {
    case AffiliationFetchResult::kSuccess:
      base::UmaHistogramTimes(
          "PasswordManager.AffiliationFetcher.FetchTime.Success", fetch_time);
      base::UmaHistogramCounts1M(
          "PasswordManager.AffiliationFetcher.ResponseSize.Success",
          response_size);
      break;
    case AffiliationFetchResult::kMalformed:
      base::UmaHistogramTimes(
          "PasswordManager.AffiliationFetcher.FetchTime.Malformed", fetch_time);
      base::UmaHistogramCounts1M(
          "PasswordManager.AffiliationFetcher.ResponseSize.Malformed",
          response_size);
      break;
    case AffiliationFetchResult::kFailure:
      base::UmaHistogramTimes(
          "PasswordManager.AffiliationFetcher.FetchTime.Failure", fetch_time);
      break;
  }
}

}  // namespace

affiliation_pb::LookupAffiliationMask CreateLookupMask(
    const AffiliationFetcherInterface::RequestInfo& request_info) {
  affiliation_pb::LookupAffiliationMask mask;

  mask.set_branding_info(request_info.branding_info);
  bool grouping_enabled =
      base::FeatureList::IsEnabled(features::kPasswordsGrouping);
  // Change password info requires grouping info enabled.
  mask.set_grouping_info(request_info.change_password_info || grouping_enabled);
  mask.set_group_branding_info(grouping_enabled);
  mask.set_change_password_info(request_info.change_password_info);
  mask.set_psl_extension_list(request_info.psl_extension_list);
  return mask;
}

AffiliationFetcherBase::AffiliationFetcherBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate)
    : url_loader_factory_(std::move(url_loader_factory)), delegate_(delegate) {}

AffiliationFetcherBase::~AffiliationFetcherBase() = default;

AffiliationFetcherDelegate* AffiliationFetcherBase::delegate() const {
  return delegate_;
}

void AffiliationFetcherBase::FinalizeRequest(
    const std::string& payload,
    const GURL& query_url,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  fetch_timer_ = base::ElapsedTimer();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = query_url;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  variations::AppendVariationsHeaderUnknownSignedIn(
      query_url, variations::InIncognito::kNo, resource_request.get());

  DCHECK(!simple_url_loader_);
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(payload, "application/x-protobuf");
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AffiliationFetcherBase::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

bool AffiliationFetcherBase::ParseResponse(
    const std::string& serialized_response,
    AffiliationFetcherDelegate::Result* result) const {
  // This function parses the response protocol buffer message for a list of
  // equivalence classes, and stores them into |results| after performing some
  // validation and sanitization steps to make sure that the contract of
  // AffiliationFetcherDelegate is fulfilled. Possible discrepancies are:
  //   * The server response will not have anything for facets that are not
  //     affiliated with any other facet, while |result| must have them.
  //   * The server response might contain future, unknown kinds of facet URIs,
  //     while |result| must contain only those that are FacetURI::is_valid().
  //   * The server response being ill-formed or self-inconsistent (in the sense
  //     that there are overlapping equivalence classes) is indicative of server
  //     side issues likely not remedied by re-fetching. Report failure in this
  //     case so the caller can be notified and it can act accordingly.
  //   * The |result| will be free of duplicate or empty equivalence classes.

  affiliation_pb::LookupAffiliationByHashPrefixResponse response;
  if (!response.ParseFromString(serialized_response)) {
    base::UmaHistogramBoolean(
        "PasswordManager.AffiliationFetcher.FailedToParseResponse", true);
    return false;
  }

  return ParseLookupAffiliationResponse(GetRequestedFacetURIs(), response,
                                        result);
}

void AffiliationFetcherBase::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  base::TimeDelta fetch_time = fetch_timer_.Elapsed();
  // Note that invoking the |delegate_| may destroy |this| synchronously, so the
  // invocation must happen last.
  bool success = simple_url_loader_->NetError() == net::OK;
  int response_code = 0;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  if (!success || net::HTTP_OK != response_code) {
    LogFetchResult(AffiliationFetchResult::kFailure, fetch_time);
    base::UmaHistogramSparse(
        "PasswordManager.AffiliationFetcher.FetchHttpResponseCode",
        response_code);
    // Network error codes are negative. See: src/net/base/net_error_list.h.
    base::UmaHistogramSparse(
        "PasswordManager.AffiliationFetcher.FetchErrorCode",
        -simple_url_loader_->NetError());
    delegate_->OnFetchFailed(this);
    return;
  }

  auto result_data = std::make_unique<AffiliationFetcherDelegate::Result>();
  if (ParseResponse(*response_body, result_data.get())) {
    LogFetchResult(AffiliationFetchResult::kSuccess, fetch_time,
                   response_body->size());
    delegate_->OnFetchSucceeded(this, std::move(result_data));
  } else {
    LogFetchResult(AffiliationFetchResult::kMalformed, fetch_time,
                   response_body->size());
    delegate_->OnMalformedResponse(this);
  }
}

bool operator==(const AffiliationFetcherInterface::RequestInfo& lhs,
                const AffiliationFetcherInterface::RequestInfo& rhs) = default;

}  // namespace password_manager
