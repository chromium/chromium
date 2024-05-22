// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector_impl.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::IsEmpty;
using testing::UnorderedElementsAreArray;

namespace password_manager {

namespace {

using StringVector = std::vector<std::string>;

// Constants to make the tests more readable.
const std::optional<PasswordHashData> NO_GAIA_OR_ENTERPRISE_REUSE =
    std::nullopt;

struct TestData {
  // Comma separated list of domains.
  std::string domains;
  std::string username;
  std::string password;
  PasswordForm::Store in_store = PasswordForm::Store::kProfileStore;
};

std::vector<TestData> GetTestDomainsPasswordsForProfileStore() {
  return {
      {"https://accounts.google.com", "gUsername", "saved_password",
       PasswordForm::Store::kProfileStore},
      {"https://facebook.com", "fbUsername", "123456789",
       PasswordForm::Store::kProfileStore},
      {"https://a.appspot.com", "appspotUsername", "abcdefghi",
       PasswordForm::Store::kProfileStore},
      {"https://twitter.com", "twitterUsername", "short",
       PasswordForm::Store::kProfileStore},
      {"https://example1.com", "example1Username", "secretword",
       PasswordForm::Store::kProfileStore},
      {"https://example2.com", "example2Username", "secretword",
       PasswordForm::Store::kProfileStore},
      {"https://example3.com", "example3Username", "123",
       PasswordForm::Store::kProfileStore},
  };
}

std::vector<TestData> GetTestDomainsPasswordsForAccountStore() {
  return {
      {"https://example4.com", "example4Username", "secretAccountPass1",
       PasswordForm::Store::kAccountStore},
      {"https://example5.com", "example5Username", "secretAccountPass2",
       PasswordForm::Store::kAccountStore},
      // Duplicated credential from profile store
      {"https://example2.com", "example2Username", "secretword",
       PasswordForm::Store::kAccountStore},
  };
}

std::unique_ptr<PasswordForm> GetForm(const std::string& domain,
                                      const std::string& username,
                                      const std::string& password,
                                      PasswordForm::Store store) {
  auto form = std::make_unique<PasswordForm>();
  form->signon_realm = domain;
  form->password_value = ASCIIToUTF16(password);
  form->username_value = ASCIIToUTF16(username);
  form->in_store = store;
  return form;
}

// Convert a vector of TestData structs into a vector of PasswordForms.
std::vector<std::unique_ptr<PasswordForm>> GetForms(
    std::vector<TestData> test_data) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  for (const auto& data : test_data) {
    // Some passwords are used on multiple domains.
    for (const auto& domain : base::SplitString(
             data.domains, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      result.push_back(
          GetForm(domain, data.username, data.password, data.in_store));
    }
  }
  return result;
}

PasswordStoreChangeList GetChangeList(
    PasswordStoreChange::Type type,
    const std::vector<std::unique_ptr<PasswordForm>>& forms) {
  PasswordStoreChangeList changes;
  for (const auto& form : forms) {
    changes.push_back(PasswordStoreChange(type, *form));
  }

  return changes;
}

std::vector<PasswordHashData> PrepareGaiaPasswordData(
    const std::vector<std::string>& passwords) {
  std::vector<PasswordHashData> result;
  for (const auto& password : passwords) {
    PasswordHashData password_hash("username_" + password,
                                   ASCIIToUTF16(password),
                                   /*force_update=*/true);
    result.push_back(password_hash);
  }
  return result;
}

std::vector<PasswordHashData> PrepareEnterprisePasswordData(
    const std::vector<std::string>& passwords) {
  std::vector<PasswordHashData> result;
  for (const auto& password : passwords) {
    PasswordHashData password_hash("enterpriseUsername_" + password,
                                   ASCIIToUTF16(password),
                                   /*force_update=*/false);
    result.push_back(password_hash);
  }
  return result;
}

void ConfigureEnterprisePasswordProtection(
    PasswordReuseDetector* reuse_detector) {
  std::optional<std::vector<GURL>> login_urls =
      std::make_optional<std::vector<GURL>>();
  login_urls->push_back(GURL("https://login.example.com"));
  reuse_detector->UseEnterprisePasswordURLs(
      login_urls, GURL("https://changepassword.example.com/"));
}

TEST(PasswordReuseDetectorTest, TypingPasswordOnDifferentSite) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"123saved_passwo", "https://evil.com",
                            &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"123saved_passwor", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  std::vector<MatchingReusedCredential> credentials = {
      {"https://accounts.google.com", u"gUsername",
       PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("saved_password"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 6, _, _));
  reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("saved_password"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 6, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);

  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  credentials = {{"https://example1.com", u"example1Username",
                  PasswordForm::Store::kProfileStore},
                 {"https://example2.com", u"example2Username",
                  PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("secretword"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 6, _, _));
  reuse_detector.CheckReuse(u"abcdsecretword", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PSLMatchNoReuseEvent) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"123456789", "https://m.facebook.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, NoPSLMatchReuseEvent) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.appspot.com", u"appspotUsername",
       PasswordForm::Store::kProfileStore}};
  // a.appspot.com and b.appspot.com are not PSL matches. So reuse event should
  // be raised.
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("abcdefghi"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 6, _, _));
  reuse_detector.CheckReuse(u"abcdefghi", "https://b.appspot.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, TooShortPasswordNoReuseEvent) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"123", "evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PasswordNotInputSuffixNoReuseEvent) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"password123", "https://evil.com", &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"123password456", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, OnLoginsChanged) {
  for (PasswordStoreChange::Type type :
       {PasswordStoreChange::ADD, PasswordStoreChange::UPDATE,
        PasswordStoreChange::REMOVE}) {
    PasswordReuseDetectorImpl reuse_detector;
    PasswordStoreChangeList changes =
        GetChangeList(type, GetForms(GetTestDomainsPasswordsForProfileStore()));
    reuse_detector.OnLoginsChanged(changes);
    MockPasswordReuseDetectorConsumer mockConsumer;

    if (type == PasswordStoreChange::REMOVE) {
      EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
    } else {
      const std::vector<MatchingReusedCredential> credentials = {
          {"https://accounts.google.com", u"gUsername",
           PasswordForm::Store::kProfileStore}};
      EXPECT_CALL(
          mockConsumer,
          OnReuseCheckDone(true, strlen("saved_password"),
                           Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                           UnorderedElementsAreArray(credentials), 6, _, _));
    }
    reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                              &mockConsumer);
  }
}

TEST(PasswordReuseDetectorTest, AddAndRemoveSameLogin) {
  PasswordReuseDetectorImpl reuse_detector;
  std::vector<std::unique_ptr<PasswordForm>> login_credentials =
      GetForms(GetTestDomainsPasswordsForProfileStore());
  // Add the test domain passwords into the saved passwords map.
  PasswordStoreChangeList add_changes =
      GetChangeList(PasswordStoreChange::ADD, login_credentials);
  reuse_detector.OnLoginsChanged(add_changes);

  const std::vector<MatchingReusedCredential>
      expected_matching_reused_credentials = {
          {"https://accounts.google.com", u"gUsername",
           PasswordForm::Store::kProfileStore}};
  MockPasswordReuseDetectorConsumer mockConsumer;
  // One of the passwords in |login_credentials| has less than the minimum
  // requirement of characters in a password so it will not be stored.
  int valid_passwords = login_credentials.size() - 1;
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("saved_password"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(expected_matching_reused_credentials),
          valid_passwords, _, _));

  // "saved_password" is a substring of "123saved_password" so it should trigger
  // a reuse and get the matching credentials.
  reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  // Remove the test domain passwords from the saved passwords map.
  PasswordStoreChangeList remove_changes =
      GetChangeList(PasswordStoreChange::REMOVE, login_credentials);
  reuse_detector.OnLoginsChanged(remove_changes);
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(/*is_reuse_found=*/false, _, _, _, _, _, _));
  // The stored credentials were removed so no reuse should be found.
  reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, AddAndRemoveSameLoginWithMultipleForms) {
  PasswordReuseDetectorImpl reuse_detector;
  // These credentials mimic a user using "secretword" on "https://example1.com"
  // and "https://example2.com" and then changing the password on
  // "https://example1.com" to "secretword1".
  std::vector<std::unique_ptr<PasswordForm>> login_credentials = GetForms({
      {"https://example1.com", "example1Username", "secretword"},
      {"https://example1.com", "example1Username", "secretword1"},
      {"https://example2.com", "example2Username", "secretword"},
  });
  // Add the test domain passwords into the saved passwords map.
  PasswordStoreChangeList add_changes =
      GetChangeList(PasswordStoreChange::ADD, login_credentials);
  reuse_detector.OnLoginsChanged(add_changes);

  std::vector<MatchingReusedCredential> expected_matching_reused_credentials = {
      {"https://example1.com", u"example1Username",
       PasswordForm::Store::kProfileStore},
      {"https://example2.com", u"example2Username",
       PasswordForm::Store::kProfileStore}};
  MockPasswordReuseDetectorConsumer mockConsumer;
  int valid_passwords = login_credentials.size();
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("secretword"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(expected_matching_reused_credentials),
          valid_passwords, _, _));

  // "secretword" is a substring of "123secretword" so it should trigger
  // a reuse and get the matching credentials.
  reuse_detector.CheckReuse(u"123secretword", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);
  // Remove two matching credentials to "secretword" from the saved passwords
  // map.
  PasswordStoreChangeList remove_changes = GetChangeList(
      PasswordStoreChange::REMOVE,
      GetForms({{"https://example1.com", "example1Username", "secretword"}}));
  reuse_detector.OnLoginsChanged(remove_changes);
  expected_matching_reused_credentials = {{"https://example2.com",
                                           u"example2Username",
                                           PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("secretword"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          testing::ElementsAreArray(expected_matching_reused_credentials), _, _,
          _));
  // Only two stored credentials were removed so reuse should still be found.
  reuse_detector.CheckReuse(u"123secretword", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  // Remove the last matching credential to "secretword" from the saved
  // passwords map.
  remove_changes = GetChangeList(
      PasswordStoreChange::REMOVE,
      GetForms({{"https://example2.com", "example2Username", "secretword"}}));
  reuse_detector.OnLoginsChanged(remove_changes);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(
                                /*is_reuse_found=*/false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"123secretword", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchMultiplePasswords) {
  // These all have different length passwods so we can check the
  // returned length.
  const std::vector<TestData> domain_passwords = {
      {"https://a.com, https://all.com", "aUsername", "34567890"},
      {"https://b.com, https://b2.com, https://all.com", "bUsername",
       "01234567890"},
      {"https://c.com, https://all.com", "cUsername", "1234567890"},
      {"https://d.com", "dUsername", "123456789"},
  };

  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://all.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://all.com", u"bUsername", PasswordForm::Store::kProfileStore},
      {"https://all.com", u"cUsername", PasswordForm::Store::kProfileStore},
      {"https://b.com", u"bUsername", PasswordForm::Store::kProfileStore},
      {"https://b2.com", u"bUsername", PasswordForm::Store::kProfileStore},
      {"https://c.com", u"cUsername", PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("01234567890"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 8, _, _));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  credentials = {
      {"https://a.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://all.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://all.com", u"cUsername", PasswordForm::Store::kProfileStore},
      {"https://c.com", u"cUsername", PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("1234567890"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 8, _, _));
  reuse_detector.CheckReuse(u"1234567890", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, GaiaPasswordNoReuse) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  reuse_detector.UseGaiaPasswordHash(
      PrepareGaiaPasswordData({"gaia_pw1", "gaia_pw2"}));

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  // Typing gaia password on https://accounts.google.com is OK.
  reuse_detector.CheckReuse(u"gaia_pw1", "https://accounts.google.com",
                            &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw2", "https://accounts.google.com",
                            &mockConsumer);
  // Only suffixes are verifed.
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"sync_password123", "https://evil.com",
                            &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"other_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, GaiaPasswordReuseFound) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<PasswordHashData> gaia_password_hashes =
      PrepareGaiaPasswordData({"gaia_pw1", "gaia_pw2"});
  std::optional<PasswordHashData> expected_reused_password_hash(
      gaia_password_hashes[0]);
  reuse_detector.UseGaiaPasswordHash(gaia_password_hashes);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("gaia_pw1"),
                               Matches(expected_reused_password_hash),
                               IsEmpty(), 6, _, _));

  reuse_detector.CheckReuse(u"gaia_pw1", "https://phishing.example.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, EnterprisePasswordNoReuse) {
  PasswordReuseDetectorImpl reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({"enterprise_pw1", "enterprise_pw2"});
  std::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[1]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  // Typing enterprise password on change password page is OK.
  reuse_detector.CheckReuse(
      u"enterprise_pw1", "https://changepassword.example.com/", &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(
      u"enterprise_pw2", "https://changepassword.example.com/", &mockConsumer);

  // Suffix match is not reuse.
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"enterprise", "https://evil.com", &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"other_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, EnterprisePasswordReuseFound) {
  PasswordReuseDetectorImpl reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({"enterprise_pw1", "enterprise_pw2"});
  std::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[1]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("enterprise_pw2"),
                               Matches(expected_reused_password_hash),
                               IsEmpty(), 6, _, _));
  reuse_detector.CheckReuse(u"enterprise_pw2", "https://phishing.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchGaiaAndMultipleSavedPasswords) {
  const std::vector<TestData> domain_passwords = {
      {"https://a.com", "aUsername", "34567890"},
      {"https://b.com", "bUsername", "01234567890"},
  };
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string gaia_password = "1234567890";
  std::vector<PasswordHashData> gaia_password_hashes =
      PrepareGaiaPasswordData({gaia_password});
  ASSERT_EQ(1u, gaia_password_hashes.size());
  std::optional<PasswordHashData> expected_reused_password_hash(
      gaia_password_hashes[0]);
  reuse_detector.UseGaiaPasswordHash(gaia_password_hashes);

  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://b.com", u"bUsername", PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("01234567890"),
                       Matches(expected_reused_password_hash),
                       UnorderedElementsAreArray(credentials), 2, _, _));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("1234567890"),
                               Matches(expected_reused_password_hash),
                               IsEmpty(), 2, _, _));
  reuse_detector.CheckReuse(u"xyz1234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchSavedPasswordButNotGaiaPassword) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string gaia_password = "gaia_password";
  reuse_detector.UseGaiaPasswordHash(PrepareGaiaPasswordData({gaia_password}));

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://accounts.google.com", u"gUsername",
       PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("saved_password"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 6, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest,
     MatchSavedPasswordButNotGaiaPasswordInAccountStore) {
  PasswordReuseDetectorImpl reuse_detector;

  auto account_store_form = std::make_unique<PasswordForm>();
  account_store_form->signon_realm = "https://twitter.com";
  account_store_form->username_value = u"twitterUsername";
  account_store_form->password_value = u"saved_password";
  account_store_form->in_store = PasswordForm::Store::kAccountStore;
  std::vector<std::unique_ptr<PasswordForm>> account_store_forms;
  account_store_forms.push_back(std::move(account_store_form));
  reuse_detector.OnGetPasswordStoreResults(std::move(account_store_forms));

  std::string gaia_password = "gaia_password";
  reuse_detector.UseGaiaPasswordHash(PrepareGaiaPasswordData({gaia_password}));

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://twitter.com", u"twitterUsername",
       PasswordForm::Store::kAccountStore}};

  MockPasswordReuseDetectorConsumer mockConsumer;
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("saved_password"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials),
                               /*saved_passwords=*/1, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchEnterpriseAndMultipleSavedPasswords) {
  const std::vector<TestData> domain_passwords = {
      {"https://a.com", "aUsername", "34567890"},
      {"https://b.com", "bUsername", "01234567890"},
  };
  PasswordReuseDetectorImpl reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string enterprise_password = "1234567890";
  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({enterprise_password});
  ASSERT_EQ(1u, enterprise_password_hashes.size());
  std::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[0]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://b.com", u"bUsername", PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("01234567890"),
                       Matches(expected_reused_password_hash),
                       UnorderedElementsAreArray(credentials), 2, _, _));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("1234567890"),
                               Matches(expected_reused_password_hash),
                               IsEmpty(), 2, _, _));
  reuse_detector.CheckReuse(u"xyz1234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchSavedPasswordButNotEnterprisePassword) {
  PasswordReuseDetectorImpl reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string enterprise_password = "enterprise_password";
  reuse_detector.UseNonGaiaEnterprisePasswordHash(
      PrepareEnterprisePasswordData({enterprise_password}));

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://accounts.google.com", u"gUsername",
       PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("saved_password"),
                       Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                       UnorderedElementsAreArray(credentials), 6, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchGaiaEnterpriseAndSavedPassword) {
  const std::vector<TestData> domain_passwords = {
      {"https://a.com", "aUsername", "34567890"},
      {"https://b.com", "bUsername", "01234567890"},
  };
  PasswordReuseDetectorImpl reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string gaia_password = "123456789";
  reuse_detector.UseGaiaPasswordHash(PrepareGaiaPasswordData({gaia_password}));

  std::string enterprise_password = "1234567890";
  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({enterprise_password});
  ASSERT_EQ(1u, enterprise_password_hashes.size());
  std::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[0]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername", PasswordForm::Store::kProfileStore},
      {"https://b.com", u"bUsername", PasswordForm::Store::kProfileStore}};
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("01234567890"),
                       Matches(expected_reused_password_hash),
                       UnorderedElementsAreArray(credentials), 2, _, _));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("1234567890"),
                               Matches(expected_reused_password_hash),
                               IsEmpty(), 2, _, _));
  reuse_detector.CheckReuse(u"xyz1234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, ClearGaiaPasswordHash) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  reuse_detector.UseGaiaPasswordHash(
      PrepareGaiaPasswordData({"gaia_pw1", "gaia_pw12"}));
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("gaia_pw1"), _, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw1", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("gaia_pw12"), _, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw12", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  reuse_detector.ClearGaiaPasswordHash("username_gaia_pw1");
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw1", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  reuse_detector.ClearAllGaiaPasswordHash();
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw12", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PasswordStoreRespectedOnRemove) {
  PasswordReuseDetectorImpl reuse_detector;

  std::vector<std::unique_ptr<PasswordForm>> profile_credentials =
      GetForms(GetTestDomainsPasswordsForProfileStore());
  std::vector<std::unique_ptr<PasswordForm>> account_credentials =
      GetForms(GetTestDomainsPasswordsForAccountStore());
  // The credential duplicated in both stores
  PasswordForm account_store_form = *account_credentials[2];

  reuse_detector.OnGetPasswordStoreResults(std::move(profile_credentials));
  reuse_detector.OnGetPasswordStoreResults(std::move(account_credentials));

  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("secretword"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(std::vector<MatchingReusedCredential>{
              {"https://example1.com", u"example1Username",
               PasswordForm::Store::kProfileStore},
              {"https://example2.com", u"example2Username",
               PasswordForm::Store::kProfileStore},
              {"https://example2.com", u"example2Username",
               PasswordForm::Store::kAccountStore}}),
          /*saved_passwords=*/9, _, _));

  reuse_detector.CheckReuse(u"secretword", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  // Simulate the removal of the account stored credential.
  PasswordStoreChangeList remove_changes;
  remove_changes.push_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, account_store_form));
  reuse_detector.OnLoginsChanged(remove_changes);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("secretword"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(std::vector<MatchingReusedCredential>{
              {"https://example1.com", u"example1Username",
               PasswordForm::Store::kProfileStore},
              {"https://example2.com", u"example2Username",
               PasswordForm::Store::kProfileStore}}),
          /*saved_passwords=*/8, _, _));
  reuse_detector.CheckReuse(u"secretword", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, AccountPasswordsCleared) {
  PasswordReuseDetectorImpl reuse_detector;

  std::vector<std::unique_ptr<PasswordForm>> profile_credentials =
      GetForms(GetTestDomainsPasswordsForProfileStore());
  std::vector<std::unique_ptr<PasswordForm>> account_credentials =
      GetForms(GetTestDomainsPasswordsForAccountStore());

  reuse_detector.OnGetPasswordStoreResults(std::move(profile_credentials));
  reuse_detector.OnGetPasswordStoreResults(std::move(account_credentials));

  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("secretword"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(std::vector<MatchingReusedCredential>{
              {"https://example1.com", u"example1Username",
               PasswordForm::Store::kProfileStore},
              {"https://example2.com", u"example2Username",
               PasswordForm::Store::kAccountStore},
              {"https://example2.com", u"example2Username",
               PasswordForm::Store::kProfileStore}}),
          /*saved_passwords=*/9, _, _));

  reuse_detector.CheckReuse(u"secretword", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  reuse_detector.ClearCachedAccountStorePasswords();

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("secretword"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(std::vector<MatchingReusedCredential>{
              {"https://example1.com", u"example1Username",
               PasswordForm::Store::kProfileStore},
              {"https://example2.com", u"example2Username",
               PasswordForm::Store::kProfileStore}}),
          /*saved_passwords=*/6, _, _));
  reuse_detector.CheckReuse(u"secretword", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, OnLoginsRetained) {
  PasswordReuseDetectorImpl reuse_detector;

  std::vector<TestData> test_data = GetTestDomainsPasswordsForProfileStore();

  reuse_detector.OnGetPasswordStoreResults(GetForms(test_data));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(true, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  // Remove the first test data entry corresponding to "saved_password".
  test_data.erase(test_data.begin());
  std::vector<PasswordForm> retained_forms;
  for (const auto& form : GetForms(test_data)) {
    retained_forms.push_back(*form);
  }
  reuse_detector.OnLoginsRetained(PasswordForm::Store::kProfileStore,
                                  retained_forms);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, OnLoginsRetainedCalledForEachStore) {
  PasswordReuseDetectorImpl reuse_detector;

  std::vector<TestData> profile_passwords =
      GetTestDomainsPasswordsForProfileStore();
  std::vector<TestData> account_passwords =
      GetTestDomainsPasswordsForAccountStore();

  reuse_detector.OnGetPasswordStoreResults(GetForms(profile_passwords));
  reuse_detector.OnGetPasswordStoreResults(GetForms(account_passwords));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(true, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  // Remove the first test data entry corresponding to "saved_password".
  profile_passwords.erase(profile_passwords.begin());
  std::vector<PasswordForm> retained_forms_in_profile_store;
  for (const auto& form : GetForms(profile_passwords)) {
    retained_forms_in_profile_store.push_back(*form);
  }
  reuse_detector.OnLoginsRetained(PasswordForm::Store::kProfileStore,
                                  retained_forms_in_profile_store);

  // Reuse found (another password was removed, not the checked one).
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(true, _, _, _, _, _, _));
  reuse_detector.CheckReuse(u"secretAccountPass1", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, ShortPasswordReuseFound) {
  PasswordReuseDetectorImpl reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(
      GetForms(GetTestDomainsPasswordsForProfileStore()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("short"), _, _, _, _, _));

  reuse_detector.CheckReuse(u"short", "https://phishing.example.com",
                            &mockConsumer);
}

}  // namespace

}  // namespace password_manager
