// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_CONSUMER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_CONSUMER_H_

#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace affiliations {

// A mock consumer of AffiliationService::GetAffiliationsAndBranding().
class MockAffiliationConsumer {
 public:
  MockAffiliationConsumer();

  MockAffiliationConsumer(const MockAffiliationConsumer&) = delete;
  MockAffiliationConsumer& operator=(const MockAffiliationConsumer&) = delete;

  ~MockAffiliationConsumer();

  // Expects that the result callback will be called exactly once and that it
  // will indicate success and return |expected_result|.
  void ExpectSuccessWithResult(const AffiliatedFacets& expected_result);

  // Expects that the result callback will be called exactly once and that it
  // will indicate a failed lookup.
  void ExpectFailure();

  AffiliationService::ResultCallback GetResultCallback();

 private:
  MOCK_METHOD2(OnResultCallback, void(const AffiliatedFacets&, bool));
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_MOCK_AFFILIATION_CONSUMER_H_
