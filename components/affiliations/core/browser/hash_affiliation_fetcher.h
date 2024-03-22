// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_HASH_AFFILIATION_FETCHER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_HASH_AFFILIATION_FETCHER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/timer/elapsed_timer.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace affiliations {

// Fetches authoritative information about facets' affiliations with additional
// privacy layer. It uses SHA-256 to hash facet URLs and sends only a specified
// amount of hash prefixes to eventually retrieve a larger group of affiliations
// including those actually required.
class HashAffiliationFetcher : public AffiliationFetcherInterface {
 public:
  HashAffiliationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate);
  ~HashAffiliationFetcher() override;

  // AffiliationFetcherInterface
  void StartRequest(const std::vector<FacetURI>& facet_uris,
                    RequestInfo request_info) override;
  const std::vector<FacetURI>& GetRequestedFacetURIs() const override;

  // Builds the URL for the Affiliation API's lookup method.
  static GURL BuildQueryURL();
  static bool IsFetchPossible();

  AffiliationFetcherDelegate* delegate() const;

 private:
  // Actually starts the request to retrieve affiliations and optionally
  // groupings for each facet in |facet_uris| along with the details based on
  // |request_info|. Calls the delegate with the results on the same thread when
  // done. If |this| is destroyed before completion, the in-flight request is
  // cancelled, and the delegate will not be called. Further details:
  //   * No cookies are sent/saved with the request.
  //   * In case of network/server errors, the request will not be retried.
  //   * Results are guaranteed to be always fresh and will never be cached.
  void FinalizeRequest(const std::string& payload,
                       const GURL& query_url,
                       net::NetworkTrafficAnnotationTag traffic_annotation);

  // Parses and validates the response protocol buffer message for a list of
  // equivalence classes, stores them into |result| and returns true on success.
  // It is guaranteed that every one of the requested Facet URIs will be a
  // member of exactly one returned equivalence class.
  // Returns false if the response was gravely ill-formed or self-inconsistent.
  // Unknown kinds of facet URIs and new protocol buffer fields will be ignored.
  bool ParseResponse(const std::string& serialized_response,
                     AffiliationFetcherDelegate::Result* result) const;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  std::vector<FacetURI> requested_facet_uris_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<AffiliationFetcherDelegate> delegate_;
  base::ElapsedTimer fetch_timer_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

bool operator==(const AffiliationFetcherInterface::RequestInfo& lhs,
                const AffiliationFetcherInterface::RequestInfo& rhs);

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_HASH_AFFILIATION_FETCHER_H_
