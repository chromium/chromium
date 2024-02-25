// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_FETCHER_FACTORY_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_FETCHER_FACTORY_H_

#include "components/affiliations/core/browser/affiliation_fetcher_factory.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace affiliations {

class MockAffiliationFetcherFactory : public AffiliationFetcherFactory {
 public:
  MockAffiliationFetcherFactory();
  ~MockAffiliationFetcherFactory() override;

  MOCK_METHOD(
      std::unique_ptr<AffiliationFetcherInterface>,
      CreateInstance,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       AffiliationFetcherDelegate* delegate),
      (override));
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_FETCHER_FACTORY_H_
