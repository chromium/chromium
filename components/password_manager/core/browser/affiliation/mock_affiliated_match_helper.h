// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

class MockAffiliatedMatchHelper : public AffiliatedMatchHelper {
 public:
  MockAffiliatedMatchHelper();
  explicit MockAffiliatedMatchHelper(AffiliationService* affiliation_service);

  MockAffiliatedMatchHelper(const MockAffiliatedMatchHelper&) = delete;
  MockAffiliatedMatchHelper& operator=(const MockAffiliatedMatchHelper&) =
      delete;

  ~MockAffiliatedMatchHelper() override;

  // Expects GetAffiliatedAndroidAndWebRealms() to be called with the
  // |expected_observed_form|, and will cause the result callback supplied to
  // GetAffiliatedAndroidAndWebRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedAndroidRealms(
      const PasswordFormDigest& expected_observed_form,
      const std::vector<std::string>& results_to_return);

 private:
  MOCK_METHOD(std::vector<std::string>,
              OnGetAffiliatedAndroidRealmsCalled,
              (const PasswordFormDigest&));

  void GetAffiliatedAndroidAndWebRealms(
      const PasswordFormDigest& observed_form,
      AffiliatedRealmsCallback result_callback) override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_
