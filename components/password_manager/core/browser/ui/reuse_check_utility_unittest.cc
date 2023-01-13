// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/reuse_check_utility.h"

#include "base/strings/utf_string_conversions.h"
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
  EXPECT_THAT(BulkReuseCheck(credentials), testing::IsEmpty());
}

TEST(ReuseCheckUtilityTest, ReuseDetected) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user1", u"password", {"https://test1.com"}));
  credentials.push_back(
      CreateCredential(u"user2", u"password", {"https://test2.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials), ElementsAre(u"password"));
}

TEST(ReuseCheckUtilityTest, ReuseDetectedSameWebsite) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user1", u"password", {"https://test.com"}));
  credentials.push_back(
      CreateCredential(u"user2", u"password", {"https://test.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials), ElementsAre(u"password"));
}

TEST(ReuseCheckUtilityTest, NoReuseIfNormalizedUsernamesEqualForSameWebsite) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://test.com"}));
  credentials.push_back(
      CreateCredential(u"UsEr", u"password", {"https://test.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials), testing::IsEmpty());
}

TEST(ReuseCheckUtilityTest, ReuseDetectedAndroidApp) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(CreateCredential(
      u"user", u"password", {"android://certificate_hash@test.com"}));
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://test.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials), ElementsAre(u"password"));
}

TEST(ReuseCheckUtilityTest, NoReuseIfWebsitesPSLMatch) {
  std::vector<CredentialUIEntry> credentials;
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://example.com"}));
  credentials.push_back(
      CreateCredential(u"user", u"password", {"https://m.example.com"}));
  EXPECT_THAT(BulkReuseCheck(credentials), testing::IsEmpty());
}

}  // namespace password_manager
