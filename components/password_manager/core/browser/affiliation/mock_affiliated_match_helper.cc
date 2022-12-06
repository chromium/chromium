// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"

#include <utility>

#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

MockAffiliatedMatchHelper::MockAffiliatedMatchHelper()
    : AffiliatedMatchHelper(nullptr) {}

MockAffiliatedMatchHelper::MockAffiliatedMatchHelper(
    AffiliationService* affiliation_service)
    : AffiliatedMatchHelper(affiliation_service) {}

MockAffiliatedMatchHelper::~MockAffiliatedMatchHelper() = default;

void MockAffiliatedMatchHelper::ExpectCallToGetAffiliatedAndroidRealms(
    const PasswordFormDigest& expected_observed_form,
    const std::vector<std::string>& results_to_return) {
  EXPECT_CALL(*this, OnGetAffiliatedAndroidRealmsCalled(expected_observed_form))
      .WillOnce(testing::Return(results_to_return));
}

void MockAffiliatedMatchHelper::GetAffiliatedAndroidAndWebRealms(
    const PasswordFormDigest& observed_form,
    AffiliatedRealmsCallback result_callback) {
  std::vector<std::string> affiliated_android_realms =
      OnGetAffiliatedAndroidRealmsCalled(observed_form);
  std::move(result_callback).Run(affiliated_android_realms);
}

}  // namespace password_manager
