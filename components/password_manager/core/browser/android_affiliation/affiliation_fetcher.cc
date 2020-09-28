// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace password_manager {

AffiliationFetcher::AffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate)
    : AffiliationFetcherBase(std::move(url_loader_factory), delegate) {}

AffiliationFetcher::~AffiliationFetcher() = default;

void AffiliationFetcher::StartRequest(const std::vector<FacetURI>& facet_uris,
                                      RequestInfo request_info) {
  requested_facet_uris_ = facet_uris;

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

  // Prepare the payload based on |facet_uris| and |request_info|.
  affiliation_pb::LookupAffiliationRequest lookup_request;
  for (const FacetURI& uri : facet_uris)
    lookup_request.add_facet(uri.canonical_spec());

  *lookup_request.mutable_mask() = CreateLookupMask(request_info);

  std::string serialized_request;
  bool success = lookup_request.SerializeToString(&serialized_request);
  DCHECK(success);

  FinalizeRequest(serialized_request, BuildQueryURL(), traffic_annotation);
}

const std::vector<FacetURI>& AffiliationFetcher::GetRequestedFacetURIs() const {
  return requested_facet_uris_;
}

// static
GURL AffiliationFetcher::BuildQueryURL() {
  return net::AppendQueryParameter(
      GURL("https://www.googleapis.com/affiliation/v1/affiliation:lookup"),
      "key", google_apis::GetAPIKey());
}

}  // namespace password_manager
