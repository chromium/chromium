// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_INTERFACE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_INTERFACE_H_

#include <vector>

#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {

class AffiliationFetcherInterface {
 public:
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
  virtual void StartRequest(const std::vector<FacetURI>& facet_uris,
                            RequestInfo request_info) = 0;

  // Returns requested facet uris.
  virtual const std::vector<FacetURI>& GetRequestedFacetURIs() const = 0;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_INTERFACE_H_
