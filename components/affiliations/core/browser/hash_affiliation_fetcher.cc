// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/affiliations/core/browser/hash_affiliation_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/features.h"
#include "components/affiliations/core/browser/lookup_affiliation_response_parser.h"
#include "components/variations/net/variations_http_headers.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace affiliations {

namespace {
constexpr int kPrefixLength = 16;

// Enumeration listing the possible outcomes of fetching affiliation information
// from the Affiliation API. This is used in UMA histograms, so do not change
// existing values, only add new values at the end.
enum class AffiliationFetchResult {
  kSuccess = 0,
  kFailure = 1,
  kMalformed = 2,
  kMaxValue = kMalformed,
};

uint64_t ComputeHashPrefix(const FacetURI& uri) {
  static_assert(kPrefixLength < 64,
                "Prefix should not be longer than 8 bytes.");

  constexpr int bytes_count = kPrefixLength / 8 + (kPrefixLength % 8 != 0);

  uint8_t hash[bytes_count];
  crypto::SHA256HashString(uri.canonical_spec(), hash, bytes_count);
  uint64_t result = 0;

  for (int i = 0; i < bytes_count; i++) {
    result <<= 8;
    result |= hash[i];
  }

  int bits_to_clear = kPrefixLength % 8;
  result &= (~0 << bits_to_clear);

  int bits_to_shift = ((8 - bytes_count) * 8);
  result <<= bits_to_shift;

  return result;
}

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
affiliation_pb::LookupAffiliationMask CreateLookupMask(
    const AffiliationFetcherInterface::RequestInfo& request_info) {
  affiliation_pb::LookupAffiliationMask mask;

  mask.set_branding_info(request_info.branding_info);
  const bool grouping_info =
      base::FeatureList::IsEnabled(features::kAffiliationsGroupInfoEnabled);
  mask.set_grouping_info(grouping_info);
  mask.set_group_branding_info(grouping_info);
  mask.set_change_password_info(request_info.change_password_info);
  mask.set_psl_extension_list(request_info.psl_extension_list);
  return mask;
}

}  // namespace

HashAffiliationFetcher::HashAffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate)
    : url_loader_factory_(std::move(url_loader_factory)), delegate_(delegate) {}

HashAffiliationFetcher::~HashAffiliationFetcher() = default;

AffiliationFetcherDelegate* HashAffiliationFetcher::delegate() const {
  return delegate_;
}

void HashAffiliationFetcher::StartRequest(
    const std::vector<FacetURI>& facet_uris,
    RequestInfo request_info) {
  requested_facet_uris_ = facet_uris;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("affiliation_lookup_by_hash", R"(
        semantics {
          sender: "Hash Affiliation Fetcher"
          description:
            " Chrome can obtain information about affiliated and grouped "
            " websites as well as link to directly change password using this "
            " request. Chrome sends only hash prefixes of the websites. "
          trigger: "Whenever a new password added or one day passed after last"
            " request for existing passwords. Another trigger is a change "
            " password action in settings."
          data:
            "Hash prefixes of websites URLs or package name for android apps."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is used to facilitate password manager filling "
            "experience by allowing users to fill passwords between "
            "affiliated sites and apps, or when user needs to get a direct"
            "change password URL. Furthermore only deleting all passwords will "
            "turn this feature off."
          policy_exception_justification:
            "Not implemented. Sending only hash prefixes to the server allows "
            "to preserve users' privacy. "
        })");

  // Prepare the payload based on |facet_uris| and |request_info|.
  affiliation_pb::LookupAffiliationByHashPrefixRequest lookup_request;

  lookup_request.set_hash_prefix_length(kPrefixLength);

  for (const FacetURI& uri : facet_uris)
    lookup_request.add_hash_prefixes(ComputeHashPrefix(uri));

  *lookup_request.mutable_mask() = CreateLookupMask(request_info);

  std::string serialized_request;
  bool success = lookup_request.SerializeToString(&serialized_request);
  DCHECK(success);

  FinalizeRequest(serialized_request, BuildQueryURL(), traffic_annotation);
}

const std::vector<FacetURI>& HashAffiliationFetcher::GetRequestedFacetURIs()
    const {
  return requested_facet_uris_;
}

// static
GURL HashAffiliationFetcher::BuildQueryURL() {
  return net::AppendQueryParameter(
      GURL("https://www.googleapis.com/affiliation/v1/"
           "affiliation:lookupByHashPrefix"),
      "key", google_apis::GetAPIKey());
}

// static
bool HashAffiliationFetcher::IsFetchPossible() {
  return google_apis::HasAPIKeyConfigured();
}

void HashAffiliationFetcher::FinalizeRequest(
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
      base::BindOnce(&HashAffiliationFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

bool HashAffiliationFetcher::ParseResponse(
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

void HashAffiliationFetcher::OnSimpleLoaderComplete(
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

}  // namespace affiliations
