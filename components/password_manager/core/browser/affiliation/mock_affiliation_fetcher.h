// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATION_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATION_FETCHER_H_

#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockAffiliationFetcher : public AffiliationFetcherInterface {
 public:
  MockAffiliationFetcher();
  ~MockAffiliationFetcher() override;

  MOCK_METHOD(void,
              StartRequest,
              (const std::vector<FacetURI>&, RequestInfo),
              (override));
  MOCK_METHOD(std::vector<FacetURI>&,
              GetRequestedFacetURIs,
              (),
              (const, override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATION_FETCHER_H_
