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

// TODO(crbug.com/40263853) Delete this class. Class should not be derived from
// the production class.
class MockAffiliatedMatchHelper : public AffiliatedMatchHelper {
 public:
  // This struct mirrors the corresponding affiliation and branding information
  // related fields from PasswordForm.
  struct AffiliationAndBrandingInformation {
    std::string affiliated_web_realm;
    std::string app_display_name;
    GURL app_icon_url;
  };

  MockAffiliatedMatchHelper();
  explicit MockAffiliatedMatchHelper(
      affiliations::AffiliationService* affiliation_service);

  MockAffiliatedMatchHelper(const MockAffiliatedMatchHelper&) = delete;
  MockAffiliatedMatchHelper& operator=(const MockAffiliatedMatchHelper&) =
      delete;

  ~MockAffiliatedMatchHelper() override;

  // Expects GetAffiliatedAndroidAndWebRealms() to be called with the
  // |expected_observed_form|, and will cause the result callback supplied to
  // GetAffiliatedAndroidAndWebRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedAndGrouped(
      const PasswordFormDigest& expected_observed_form,
      std::vector<std::string> affiliated_realms,
      std::vector<std::string> grouped_realms = {});

  // Expects GetGroup() to be called with the
  // |expected_observed_form|, and will cause the result callback supplied to
  // GetGroup() to be invoked with
  // |results_to_return|.
  void ExpectCallToGetGroup(const PasswordFormDigest& expected_observed_form,
                            const std::vector<std::string>& results_to_return);

  void ExpectCallToInjectAffiliationAndBrandingInformation(
      const std::vector<AffiliationAndBrandingInformation>& results_to_inject);

 private:
  MOCK_METHOD(std::vector<std::string>,
              OnGetAffiliatedAndroidRealmsCalled,
              (const PasswordFormDigest&));

  MOCK_METHOD(std::vector<std::string>,
              OnGetGroup,
              (const PasswordFormDigest&));

  MOCK_METHOD(std::vector<AffiliationAndBrandingInformation>,
              OnInjectAffiliationAndBrandingInformationCalled,
              ());

  void GetAffiliatedAndGroupedRealms(
      const PasswordFormDigest& observed_form,
      AffiliatedRealmsCallback result_callback) override;

  void InjectAffiliationAndBrandingInformation(
      LoginsResult forms,
      base::OnceCallback<void(LoginsResultOrError)> result_callback) override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_
