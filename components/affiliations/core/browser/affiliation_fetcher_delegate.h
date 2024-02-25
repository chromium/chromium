// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_DELEGATE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_DELEGATE_H_

#include <memory>
#include <vector>

#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {
class AffiliationFetcherInterface;

// Interface that users of AffiliationFetcher should implement to get results of
// the fetch. It is safe to destroy the fetcher in any of the event handlers.
class AffiliationFetcherDelegate {
 public:
  // Encapsulates the response to an affiliations request.
  struct Result {
    Result();
    Result(const Result& other);
    Result(Result&& other);
    Result& operator=(const Result& other);
    Result& operator=(Result&& other);
    ~Result();

    std::vector<AffiliatedFacets> affiliations;
    std::vector<GroupedFacets> groupings;
    std::vector<std::string> psl_extensions;
  };

  // Called when affiliation information has been successfully retrieved. The
  // |result| will contain at most as many equivalence class as facet URIs in
  // the request, and each requested facet URI will appear in exactly one
  // equivalence class.
  virtual void OnFetchSucceeded(AffiliationFetcherInterface* fetcher,
                                std::unique_ptr<Result> result) = 0;

  // Called when affiliation information could not be fetched due to a network
  // error or a presumably transient server error. The implementor may and will
  // probably want to retry the request (once network connectivity is
  // re-established, and/or with exponential back-off).
  virtual void OnFetchFailed(AffiliationFetcherInterface* fetcher) = 0;

  // Called when an affiliation response was received, but it was either gravely
  // ill-formed or self-inconsistent. It is likely that a repeated fetch would
  // yield the same, erroneous response, therefore, to avoid overloading the
  // server, the fetch must not be repeated in the short run.
  virtual void OnMalformedResponse(AffiliationFetcherInterface* fetcher) = 0;

 protected:
  virtual ~AffiliationFetcherDelegate() = default;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_DELEGATE_H_
