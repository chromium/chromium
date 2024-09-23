// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_FETCHER_DELEGATE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_FETCHER_DELEGATE_H_

#include <memory>

#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace affiliations {
class AffiliationFetcherInterface;

class MockAffiliationFetcherDelegate : public AffiliationFetcherDelegate {
 public:
  MockAffiliationFetcherDelegate();
  ~MockAffiliationFetcherDelegate() override;

  MOCK_METHOD(void,
              OnFetchSucceeded,
              (AffiliationFetcherInterface * fetcher,
               std::unique_ptr<Result> result),
              (override));
  MOCK_METHOD(void,
              OnFetchFailed,
              (AffiliationFetcherInterface * fetcher),
              (override));
  MOCK_METHOD(void,
              OnMalformedResponse,
              (AffiliationFetcherInterface * fetcher),
              (override));
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_FETCHER_DELEGATE_H_
