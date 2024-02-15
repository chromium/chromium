// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_FACET_MANAGER_HOST_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_FACET_MANAGER_HOST_H_

#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {

// Interface through which FacetManagers can access shared functionality
// provided by the AffiliationBackend.
class FacetManagerHost {
 public:
  virtual ~FacetManagerHost() = default;

  // Reads the equivalence class containing |facet_uri| from the database and
  // returns true if found; returns false otherwise.
  virtual bool ReadAffiliationsAndBrandingFromDatabase(
      const FacetURI& facet_uri,
      AffiliatedFacetsWithUpdateTime* affiliations) = 0;

  // Signals the fetching logic that affiliation information for a facet needs
  // to be fetched immediately.
  virtual void SignalNeedNetworkRequest() = 0;

  // Requests that the FacetManager corresponding to |facet_uri| be notified at
  // the specified |time| so it can perform delayed administrative tasks.
  virtual void RequestNotificationAtTime(const FacetURI& facet_uri,
                                         base::Time time) = 0;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_FACET_MANAGER_HOST_H_
