// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/mock_affiliation_consumer.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"

namespace affiliations {

MockAffiliationConsumer::MockAffiliationConsumer() {
  EXPECT_CALL(*this, OnResultCallback(testing::_, testing::_)).Times(0);
}

MockAffiliationConsumer::~MockAffiliationConsumer() = default;

void MockAffiliationConsumer::ExpectSuccessWithResult(
    const AffiliatedFacets& expected_result) {
  EXPECT_CALL(*this, OnResultCallback(
                         testing::UnorderedElementsAreArray(expected_result),
                         true)).Times(1);
}

void MockAffiliationConsumer::ExpectFailure() {
  EXPECT_CALL(*this, OnResultCallback(testing::UnorderedElementsAre(), false))
      .Times(1);
}

AffiliationService::ResultCallback
MockAffiliationConsumer::GetResultCallback() {
  return base::BindOnce(&MockAffiliationConsumer::OnResultCallback,
                        base::Unretained(this));
}

}  // namespace affiliations
