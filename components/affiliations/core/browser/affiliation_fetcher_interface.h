// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_INTERFACE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_INTERFACE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "net/http/http_status_code.h"

namespace affiliations {

class AffiliationFetcherInterface {
 public:
  // Encapsulates the response to an affiliations request.
  struct ParsedFetchResponse {
    ParsedFetchResponse();
    ParsedFetchResponse(const ParsedFetchResponse& other);
    ParsedFetchResponse(ParsedFetchResponse&& other);
    ParsedFetchResponse& operator=(const ParsedFetchResponse& other);
    ParsedFetchResponse& operator=(ParsedFetchResponse&& other);
    ~ParsedFetchResponse();

    std::vector<AffiliatedFacets> affiliations;
    std::vector<GroupedFacets> groupings;
    std::vector<std::string> psl_extensions;
  };

  // Encapsulates the result of a fetch over network.
  struct FetchResult {
    FetchResult();
    FetchResult(const FetchResult& other);
    FetchResult(FetchResult&& other);
    FetchResult& operator=(const FetchResult& other);
    FetchResult& operator=(FetchResult&& other);
    ~FetchResult();

    bool IsSuccessful() const;

    std::optional<ParsedFetchResponse> data;
    int network_status;
    std::optional<net::HttpStatusCode> http_status_code;
  };
  // A struct that enables to set Affiliation Fetcher request mask.
  struct RequestInfo {
    bool branding_info = false;
    bool change_password_info = false;
    bool psl_extension_list = false;

    friend bool operator==(const RequestInfo&, const RequestInfo&);
  };

  AffiliationFetcherInterface() = default;
  virtual ~AffiliationFetcherInterface() = default;

  AffiliationFetcherInterface(const AffiliationFetcherInterface&) = delete;
  AffiliationFetcherInterface& operator=(const AffiliationFetcherInterface&) =
      delete;
  AffiliationFetcherInterface(AffiliationFetcherInterface&&) = delete;
  AffiliationFetcherInterface& operator=(AffiliationFetcherInterface&&) =
      delete;

  // Starts the request to retrieve affiliations for each facet in
  // |facet_uris|.
  // This will run |result_callback| on a successful or a failed fetch,
  // including if the fetcher had been destroyed before its fetch finished.
  virtual void StartRequest(
      const std::vector<FacetURI>& facet_uris,
      RequestInfo request_info,
      base::OnceCallback<void(FetchResult)> result_callback) = 0;

  // Returns requested facet uris.
  virtual const std::vector<FacetURI>& GetRequestedFacetURIs() const = 0;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_INTERFACE_H_
