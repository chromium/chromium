// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/android_affiliation/lookup_affiliation_response_parser.h"
#include "components/password_manager/core/browser/android_affiliation/test_affiliation_fetcher_factory.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace password_manager {

// Enumeration listing the possible outcomes of fetching affiliation information
// from the Affiliation API. This is used in UMA histograms, so do not change
// existing values, only add new values at the end.
enum AffiliationFetchResult {
  AFFILIATION_FETCH_RESULT_SUCCESS,
  AFFILIATION_FETCH_RESULT_FAILURE,
  AFFILIATION_FETCH_RESULT_MALFORMED,
  AFFILIATION_FETCH_RESULT_MAX
};

static TestAffiliationFetcherFactory* g_testing_factory = nullptr;

AffiliationFetcher::AffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::vector<FacetURI>& facet_uris,
    AffiliationFetcherDelegate* delegate)
    : url_loader_factory_(std::move(url_loader_factory)),
      requested_facet_uris_(facet_uris),
      delegate_(delegate) {
  for (const FacetURI& uri : requested_facet_uris_) {
    DCHECK(uri.is_valid());
  }
}

AffiliationFetcher::~AffiliationFetcher() = default;

// static
AffiliationFetcher* AffiliationFetcher::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::vector<FacetURI>& facet_uris,
    AffiliationFetcherDelegate* delegate) {
  if (g_testing_factory) {
    return g_testing_factory->CreateInstance(std::move(url_loader_factory),
                                             facet_uris, delegate);
  }
  return new AffiliationFetcher(std::move(url_loader_factory), facet_uris,
                                delegate);
}

// static
void AffiliationFetcher::SetFactoryForTesting(
    TestAffiliationFetcherFactory* factory) {
  g_testing_factory = factory;
}

void AffiliationFetcher::StartRequest() {
  DCHECK(!simple_url_loader_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("affiliation_lookup", R"(
        semantics {
          sender: "Android Credentials Affiliation Fetcher"
          description:
            "Users syncing their passwords may have credentials stored for "
            "Android apps. Unless synced data is encrypted with a custom "
            "passphrase, this service downloads the associations between "
            "Android apps and the corresponding websites. Thus, the Android "
            "credentials can be used while browsing the web. "
          trigger: "Periodically in the background."
          data:
            "List of Android apps the user has credentials for. The passwords "
            "and usernames aren't sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature either by stoping "
            "syncing passwords to Google (via unchecking 'Passwords' in "
            "Chromium's settings under 'Sign In', 'Advanced sync settings') or "
            "by introducing a custom passphrase to disable this service. The "
            "feature is enabled by default."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = BuildQueryURL();
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(PreparePayload(),
                                            "application/x-protobuf");
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AffiliationFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

// static
GURL AffiliationFetcher::BuildQueryURL() {
  return net::AppendQueryParameter(
      GURL("https://www.googleapis.com/affiliation/v1/affiliation:lookup"),
      "key", google_apis::GetAPIKey());
}

std::string AffiliationFetcher::PreparePayload() const {
  affiliation_pb::LookupAffiliationRequest lookup_request;
  for (const FacetURI& uri : requested_facet_uris_)
    lookup_request.add_facet(uri.canonical_spec());

  // Enable request for branding information.
  auto mask = std::make_unique<affiliation_pb::LookupAffiliationMask>();
  mask->set_branding_info(true);
  lookup_request.set_allocated_mask(mask.release());

  std::string serialized_request;
  bool success = lookup_request.SerializeToString(&serialized_request);
  DCHECK(success);
  return serialized_request;
}

bool AffiliationFetcher::ParseResponse(
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

  affiliation_pb::LookupAffiliationResponse response;
  if (!response.ParseFromString(serialized_response))
    return false;

  return ParseLookupAffiliationResponse(requested_facet_uris_, response,
                                        result);
}

void AffiliationFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  // Note that invoking the |delegate_| may destroy |this| synchronously, so the
  // invocation must happen last.
  std::unique_ptr<AffiliationFetcherDelegate::Result> result_data(
      new AffiliationFetcherDelegate::Result);
  if (response_body) {
    if (ParseResponse(*response_body, result_data.get())) {
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AffiliationFetcher.FetchResult",
          AFFILIATION_FETCH_RESULT_SUCCESS, AFFILIATION_FETCH_RESULT_MAX);
      delegate_->OnFetchSucceeded(std::move(result_data));
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AffiliationFetcher.FetchResult",
          AFFILIATION_FETCH_RESULT_MALFORMED, AFFILIATION_FETCH_RESULT_MAX);
      delegate_->OnMalformedResponse();
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.AffiliationFetcher.FetchResult",
                              AFFILIATION_FETCH_RESULT_FAILURE,
                              AFFILIATION_FETCH_RESULT_MAX);
    int response_code = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    base::UmaHistogramSparse(
        "PasswordManager.AffiliationFetcher.FetchHttpResponseCode",
        response_code);
    // Network error codes are negative. See: src/net/base/net_error_list.h.
    base::UmaHistogramSparse(
        "PasswordManager.AffiliationFetcher.FetchErrorCode",
        -simple_url_loader_->NetError());
    delegate_->OnFetchFailed();
  }
}

}  // namespace password_manager
