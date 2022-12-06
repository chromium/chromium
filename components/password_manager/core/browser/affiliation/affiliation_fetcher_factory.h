// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace password_manager {

class AffiliationFetcherInterface;
class AffiliationFetcherDelegate;

// Interface for a factory to construct instances of AffiliationFetcher
// subclasses.
class AffiliationFetcherFactory {
 public:
  AffiliationFetcherFactory() = default;
  virtual ~AffiliationFetcherFactory() = default;

  AffiliationFetcherFactory(const AffiliationFetcherFactory&) = delete;
  AffiliationFetcherFactory& operator=(const AffiliationFetcherFactory&) =
      delete;
  AffiliationFetcherFactory(AffiliationFetcherFactory&&) = delete;
  AffiliationFetcherFactory& operator=(AffiliationFetcherFactory&&) = delete;

  // Constructs a fetcher to retrieve affiliations for requested facets
  // using the specified |url_loader_factory|, and will provide the results
  // to the |delegate| on the same thread that creates the instance.
  virtual std::unique_ptr<AffiliationFetcherInterface> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_FACTORY_H_
