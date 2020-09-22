// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_interface.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace password_manager {

// Fetches authoritative information regarding which facets are affiliated with
// each other, that is, which facets belong to the same logical application.
// See affiliation_utils.h for a definition of what this means.
//
// An instance is good for exactly one fetch, and may be used from any thread
// that runs a message loop (i.e. not a worker pool thread).
class AffiliationFetcher : public AffiliationFetcherInterface {
 public:
  AffiliationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate);
  ~AffiliationFetcher() override;

  // Builds the URL for the Affiliation API's lookup method.
  static GURL BuildQueryURL();

  // Actually starts the request to retrieve affiliations and optionally
  // groupings for each facet in |facet_uris| along with the details based on
  // |request_info|. Calls the delegate with the results on the same thread when
  // done. If |this| is destroyed before completion, the in-flight request is
  // cancelled, and the delegate will not be called. Further details:
  //   * No cookies are sent/saved with the request.
  //   * In case of network/server errors, the request will not be retried.
  //   * Results are guaranteed to be always fresh and will never be cached.
  void StartRequest(const std::vector<FacetURI>& facet_uris,
                    RequestInfo request_info) override;

  const std::vector<FacetURI>& GetRequestedFacetURIs() const override;

  AffiliationFetcherDelegate* delegate() const;

 private:
  // Prepares and returns the serialized protocol buffer message that will be
  // the payload of the POST request. Sets mask request based on |request_info|.
  std::string PreparePayload(RequestInfo request_info) const;

  // Parses and validates the response protocol buffer message for a list of
  // equivalence classes, stores them into |result| and returns true on success.
  // It is guaranteed that every one of the requested Facet URIs will be a
  // member of exactly one returned equivalence class.
  // Returns false if the response was gravely ill-formed or self-inconsistent.
  // Unknown kinds of facet URIs and new protocol buffer fields will be ignored.
  bool ParseResponse(const std::string& serialized_response,
                     AffiliationFetcherDelegate::Result* result) const;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<FacetURI> requested_facet_uris_;
  AffiliationFetcherDelegate* const delegate_;
  // Timer to track the response time of the request.
  base::ElapsedTimer fetch_timer_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_
