// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_INTERFACE_H_

#include <vector>

#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"

namespace password_manager {

class AffiliationFetcherInterface {
 public:
  // A struct that enables to set Affiliation Fetcher request mask.
  struct RequestInfo {
    bool branding_info = false;
    bool change_password_info = false;

    bool operator==(const RequestInfo& other) const {
      return branding_info == other.branding_info &&
             change_password_info == other.change_password_info;
    }
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

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_INTERFACE_H_
