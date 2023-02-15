// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_BASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_interface.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace password_manager {

// Creates lookup mask based on |request_info|.
affiliation_pb::LookupAffiliationMask CreateLookupMask(
    const AffiliationFetcherInterface::RequestInfo& request_info);

// A base class for affiliation fetcher. Should not be used directly.
//
// Fetches authoritative information regarding which facets are affiliated with
// each other, that is, which facets belong to the same logical application.
// Apart from affiliations the service also supports groups and other details,
// all of which have to be specified when starting a request.
// See affiliation_utils.h for the definitions.
//
// An instance is good for exactly one fetch, and may be used from any thread
// that runs a message loop (i.e. not a worker pool thread).
class AffiliationFetcherBase : public virtual AffiliationFetcherInterface {
 public:
  ~AffiliationFetcherBase() override;

  AffiliationFetcherDelegate* delegate() const;

 protected:
  AffiliationFetcherBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate);

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

 private:
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
  const raw_ptr<AffiliationFetcherDelegate> delegate_;
  base::ElapsedTimer fetch_timer_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

bool operator==(const AffiliationFetcherInterface::RequestInfo& lhs,
                const AffiliationFetcherInterface::RequestInfo& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_BASE_H_
