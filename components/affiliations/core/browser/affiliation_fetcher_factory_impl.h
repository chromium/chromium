// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_FACTORY_IMPL_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_FACTORY_IMPL_H_

#include "components/affiliations/core/browser/affiliation_fetcher_factory.h"

namespace affiliations {

class AffiliationFetcherFactoryImpl : public AffiliationFetcherFactory {
 public:
  AffiliationFetcherFactoryImpl();
  ~AffiliationFetcherFactoryImpl() override;

  std::unique_ptr<AffiliationFetcherInterface> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate) override;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_FACTORY_IMPL_H_
