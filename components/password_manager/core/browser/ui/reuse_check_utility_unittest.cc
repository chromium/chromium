// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/reuse_check_utility.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::ElementsAre;

CredentialUIEntry CreateCredential(
    const std::u16string& username,
    const std::u16string& password,
    const std::vector<std::string>& signon_realms) {
  CredentialUIEntry credential;
  credential.username = username;
  credential.password = password;
  base::ranges::transform(signon_realms, std::back_inserter(credential.facets),
                          [](const std::string& signon_realm) {
                            CredentialFacet facet;
                            facet.signon_realm = signon_realm;
                            return facet;
                          });
  return credential;
}

}  // namespace

TEST(ReuseCheckUtilityTest, CheckNoReuse) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user1", u"password1", {"https://test1.com"}));
  credentials.push_back(
      CreateCredential(u"user2", u"password2", {"https://test2.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials, {}), testing::IsEmpty());
}

TEST(ReuseCheckUtilityTest, ReuseDetected) {
  base::HistogramTester histogram_tester;

  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user1", u"password", {"https://test1.com"}));
  credentials.push_back(
      CreateCredential(u"user2", u"password", {"https://test2.com"}));
  credentials.push_back(
      CreateCredential(u"user", u"password2", {"https://test3.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials, {}), ElementsAre(u"password"));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ReuseCheck.CheckedPasswords", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ReuseCheck.ReusedPasswords", 1, 1);
}

TEST(ReuseCheckUtilityTest, ReuseDetectedSameWebsite) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user1", u"password", {"https://test.com"}));
  credentials.push_back(
      CreateCredential(u"user2", u"password", {"https://test.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials, {}), ElementsAre(u"password"));
}

TEST(ReuseCheckUtilityTest, NoReuseIfNormalizedUsernamesEqualForSameWebsite) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://test.com"}));
  credentials.push_back(
      CreateCredential(u"UsEr", u"password", {"https://test.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials, {}), testing::IsEmpty());
}

TEST(ReuseCheckUtilityTest, ReuseDetectedAndroidApp) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(CreateCredential(
      u"user", u"password", {"android://certificate_hash@test.com"}));
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://test.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials, {}), ElementsAre(u"password"));
}

TEST(ReuseCheckUtilityTest, NoReuseIfWebsitesPSLMatch) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://example.com"}));
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://m.example.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials, {}), testing::IsEmpty());
}

TEST(ReuseCheckUtilityTest, NoReuseIfFromTheSameAffiliatedGroup) {
  AffiliatedGroup affiliated_group(
      {CreateCredential(u"Jan", u"password", {"https://example.com"}),
       CreateCredential(u"Mohamed", u"password",
                        {"android://certificate_hash@test.com"})},
      affiliations::FacetBrandingInfo());

  std::vector<CredentialUIEntry> credentials;
  credentials.insert(credentials.end(),
                     affiliated_group.GetCredentials().begin(),
                     affiliated_group.GetCredentials().end());
  EXPECT_THAT(BulkReuseCheck(credentials, {affiliated_group}),
              testing::IsEmpty());
}

}  // namespace password_manager
