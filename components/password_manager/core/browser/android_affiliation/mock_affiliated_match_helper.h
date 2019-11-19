// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class MockAffiliatedMatchHelper : public AffiliatedMatchHelper {
 public:
  // This struct mirrors the corresponding affiliation and branding information
  // related fields from autofill::PasswordForm.
  struct AffiliationAndBrandingInformation {
    std::string affiliated_web_realm;
    std::string app_display_name;
    GURL app_icon_url;
  };

  MockAffiliatedMatchHelper();
  ~MockAffiliatedMatchHelper() override;

  // Expects GetAffiliatedAndroidRealms() to be called with the
  // |expected_observed_form|, and will cause the result callback supplied to
  // GetAffiliatedAndroidRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedAndroidRealms(
      const PasswordStore::FormDigest& expected_observed_form,
      const std::vector<std::string>& results_to_return);

  // Expects GetAffiliatedWebRealms() to be called with the
  // |expected_android_form|, and will cause the result callback supplied to
  // GetAffiliatedWebRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedWebRealms(
      const PasswordStore::FormDigest& expected_android_form,
      const std::vector<std::string>& results_to_return);

  void ExpectCallToInjectAffiliationAndBrandingInformation(
      const std::vector<AffiliationAndBrandingInformation>& results_to_inject);

 private:
  MOCK_METHOD1(OnGetAffiliatedAndroidRealmsCalled,
               std::vector<std::string>(const PasswordStore::FormDigest&));
  MOCK_METHOD1(OnGetAffiliatedWebRealmsCalled,
               std::vector<std::string>(const PasswordStore::FormDigest&));
  MOCK_METHOD0(OnInjectAffiliationAndBrandingInformationCalled,
               std::vector<AffiliationAndBrandingInformation>());

  void GetAffiliatedAndroidRealms(
      const PasswordStore::FormDigest& observed_form,
      AffiliatedRealmsCallback result_callback) override;
  void GetAffiliatedWebRealms(
      const PasswordStore::FormDigest& android_form,
      AffiliatedRealmsCallback result_callback) override;

  void InjectAffiliationAndBrandingInformation(
      std::vector<std::unique_ptr<autofill::PasswordForm>> forms,
      PasswordFormsCallback result_callback) override;

  DISALLOW_COPY_AND_ASSIGN(MockAffiliatedMatchHelper);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_MOCK_AFFILIATED_MATCH_HELPER_H_
