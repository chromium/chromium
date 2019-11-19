// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
constexpr char kTestProxyOrigin[] = "http://proxy.com/";
constexpr char kTestProxySignonRealm[] = "proxy.com/realm";
constexpr char kTestURL[] = "https://example.com/login/";
constexpr char kTestUsername[] = "Username";
constexpr char kTestUsername2[] = "Username2";
constexpr char kTestPassword[] = "12345";

autofill::PasswordForm GetTestAndroidCredential() {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.origin = GURL(kTestAndroidRealm);
  form.signon_realm = kTestAndroidRealm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

autofill::PasswordForm GetTestCredential() {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.origin = GURL(kTestURL);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

autofill::PasswordForm GetTestProxyCredential() {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kBasic;
  form.origin = GURL(kTestProxyOrigin);
  form.signon_realm = kTestProxySignonRealm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

}  // namespace

using password_manager::UnorderedPasswordFormElementsAre;
using testing::_;
using testing::DoAll;
using testing::Return;

TEST(PasswordManagerUtil, TrimUsernameOnlyCredentials) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms;
  std::vector<std::unique_ptr<autofill::PasswordForm>> expected_forms;
  forms.push_back(
      std::make_unique<autofill::PasswordForm>(GetTestAndroidCredential()));
  expected_forms.push_back(
      std::make_unique<autofill::PasswordForm>(GetTestAndroidCredential()));

  autofill::PasswordForm username_only;
  username_only.scheme = autofill::PasswordForm::Scheme::kUsernameOnly;
  username_only.signon_realm = kTestAndroidRealm;
  username_only.username_value = base::ASCIIToUTF16(kTestUsername2);
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));

  username_only.federation_origin =
      url::Origin::Create(GURL(kTestFederationURL));
  username_only.skip_zero_click = false;
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));
  username_only.skip_zero_click = true;
  expected_forms.push_back(
      std::make_unique<autofill::PasswordForm>(username_only));

  TrimUsernameOnlyCredentials(&forms);

  EXPECT_THAT(forms, UnorderedPasswordFormElementsAre(&expected_forms));
}

TEST(PasswordManagerUtil, GetSignonRealmWithProtocolExcluded) {
  autofill::PasswordForm http_form;
  http_form.origin = GURL("http://www.google.com/page-1/");
  http_form.signon_realm = "http://www.google.com/";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(http_form), "www.google.com/");

  autofill::PasswordForm https_form;
  https_form.origin = GURL("https://www.google.com/page-1/");
  https_form.signon_realm = "https://www.google.com/";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(https_form), "www.google.com/");

  autofill::PasswordForm federated_form;
  federated_form.origin = GURL("http://localhost:8000/");
  federated_form.signon_realm =
      "federation://localhost/accounts.federation.com";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(federated_form),
            "localhost/accounts.federation.com");
}

TEST(PasswordManagerUtil, FindBestMatches) {
  const int kNotFound = -1;
  struct TestMatch {
    bool is_psl_match;
    bool preferred;
    std::string username;
  };
  struct TestCase {
    const char* description;
    std::vector<TestMatch> matches;
    int expected_preferred_match_index;
    std::map<std::string, size_t> expected_best_matches_indices;
  } test_cases[] = {
      {"Empty matches", {}, kNotFound, {}},
      {"1 preferred non-psl match",
       {{.is_psl_match = false, .preferred = true, .username = "u"}},
       0,
       {{"u", 0}}},
      {"1 non-preferred psl match",
       {{.is_psl_match = true, .preferred = false, .username = "u"}},
       0,
       {{"u", 0}}},
      {"2 matches with the same username",
       {{.is_psl_match = false, .preferred = false, .username = "u"},
        {.is_psl_match = false, .preferred = true, .username = "u"}},
       1,
       {{"u", 1}}},
      {"2 matches with different usernames, preferred taken",
       {{.is_psl_match = false, .preferred = false, .username = "u1"},
        {.is_psl_match = false, .preferred = true, .username = "u2"}},
       1,
       {{"u1", 0}, {"u2", 1}}},
      {"2 matches with different usernames, non-psl much taken",
       {{.is_psl_match = false, .preferred = false, .username = "u1"},
        {.is_psl_match = true, .preferred = true, .username = "u2"}},
       0,
       {{"u1", 0}, {"u2", 1}}},
      {"8 matches, 3 usernames",
       {{.is_psl_match = false, .preferred = false, .username = "u2"},
        {.is_psl_match = true, .preferred = false, .username = "u3"},
        {.is_psl_match = true, .preferred = false, .username = "u1"},
        {.is_psl_match = false, .preferred = true, .username = "u3"},
        {.is_psl_match = true, .preferred = false, .username = "u1"},
        {.is_psl_match = false, .preferred = false, .username = "u2"},
        {.is_psl_match = true, .preferred = true, .username = "u3"},
        {.is_psl_match = false, .preferred = false, .username = "u1"}},
       3,
       {{"u1", 7}, {"u2", 0}, {"u3", 3}}},

  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.description);
    // Convert TestMatch to PasswordForm.
    std::vector<PasswordForm> owning_matches;
    for (const TestMatch match : test_case.matches) {
      PasswordForm form;
      form.is_public_suffix_match = match.is_psl_match;
      form.preferred = match.preferred;
      form.username_value = base::ASCIIToUTF16(match.username);
      owning_matches.push_back(form);
    }
    std::vector<const PasswordForm*> matches;
    for (const PasswordForm& match : owning_matches)
      matches.push_back(&match);

    std::vector<const PasswordForm*> best_matches;
    const PasswordForm* preferred_match = nullptr;

    std::vector<const PasswordForm*> same_scheme_matches;
    FindBestMatches(matches, PasswordForm::Scheme::kHtml,
                    /*sort_matches_by_date_last_used=*/false,
                    &same_scheme_matches, &best_matches, &preferred_match);

    if (test_case.expected_preferred_match_index == kNotFound) {
      // Case of empty |matches|.
      EXPECT_FALSE(preferred_match);
      EXPECT_TRUE(best_matches.empty());
    } else {
      // Check |preferred_match|.
      EXPECT_EQ(matches[test_case.expected_preferred_match_index],
                preferred_match);
      // Check best matches.
      ASSERT_EQ(test_case.expected_best_matches_indices.size(),
                best_matches.size());

      for (const PasswordForm* match : best_matches) {
        std::string username = base::UTF16ToUTF8(match->username_value);
        ASSERT_NE(test_case.expected_best_matches_indices.end(),
                  test_case.expected_best_matches_indices.find(username));
        size_t expected_index =
            test_case.expected_best_matches_indices.at(username);
        size_t actual_index = std::distance(
            matches.begin(), std::find(matches.begin(), matches.end(), match));
        EXPECT_EQ(expected_index, actual_index);
      }
    }
  }
}

TEST(PasswordManagerUtil, FindBestMatchesByUsageTime) {
  const base::Time kNow = base::Time::Now();
  const base::Time kYesterday = kNow - base::TimeDelta::FromDays(1);
  const base::Time k2DaysAgo = kNow - base::TimeDelta::FromDays(2);
  const int kNotFound = -1;
  struct TestMatch {
    bool is_psl_match;
    bool preferred;
    base::Time date_last_used;
    std::string username;
  };
  struct TestCase {
    const char* description;
    std::vector<TestMatch> matches;
    int expected_preferred_match_index;
    std::map<std::string, size_t> expected_best_matches_indices;
  } test_cases[] = {
      {"Empty matches", {}, kNotFound, {}},
      {"1 preferred non-psl match",
       {{.is_psl_match = false,
         .preferred = true,
         .date_last_used = kNow,
         .username = "u"}},
       0,
       {{"u", 0}}},
      {"1 non-preferred psl match",
       {{.is_psl_match = true,
         .preferred = false,
         .date_last_used = kNow,
         .username = "u"}},
       0,
       {{"u", 0}}},
      {"2 matches with the same username",
       {{.is_psl_match = false,
         .preferred = false,
         .date_last_used = kNow,
         .username = "u"},
        {.is_psl_match = false,
         .preferred = true,
         .date_last_used = kYesterday,
         .username = "u"}},
       0,
       {{"u", 0}}},
      {"2 matches with different usernames, most recently used taken",
       {{.is_psl_match = false,
         .preferred = false,
         .date_last_used = kNow,
         .username = "u1"},
        {.is_psl_match = false,
         .preferred = true,
         .date_last_used = kYesterday,
         .username = "u2"}},
       0,
       {{"u1", 0}, {"u2", 1}}},
      {"2 matches with different usernames, non-psl much taken",
       {{.is_psl_match = false,
         .preferred = false,
         .date_last_used = kYesterday,
         .username = "u1"},
        {.is_psl_match = true,
         .preferred = true,
         .date_last_used = kNow,
         .username = "u2"}},
       0,
       {{"u1", 0}, {"u2", 1}}},
      {"8 matches, 3 usernames",
       {{.is_psl_match = false,
         .preferred = false,
         .date_last_used = kYesterday,
         .username = "u2"},
        {.is_psl_match = true,
         .preferred = false,
         .date_last_used = kYesterday,
         .username = "u3"},
        {.is_psl_match = true,
         .preferred = false,
         .date_last_used = kYesterday,
         .username = "u1"},
        {.is_psl_match = false,
         .preferred = true,
         .date_last_used = k2DaysAgo,
         .username = "u3"},
        {.is_psl_match = true,
         .preferred = false,
         .date_last_used = kNow,
         .username = "u1"},
        {.is_psl_match = false,
         .preferred = false,
         .date_last_used = kNow,
         .username = "u2"},
        {.is_psl_match = true,
         .preferred = true,
         .date_last_used = kYesterday,
         .username = "u3"},
        {.is_psl_match = false,
         .preferred = false,
         .date_last_used = k2DaysAgo,
         .username = "u1"}},
       5,
       {{"u1", 7}, {"u2", 5}, {"u3", 3}}},

  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.description);
    // Convert TestMatch to PasswordForm.
    std::vector<PasswordForm> owning_matches;
    for (const TestMatch& match : test_case.matches) {
      PasswordForm form;
      form.is_public_suffix_match = match.is_psl_match;
      form.preferred = match.preferred;
      form.date_last_used = match.date_last_used;
      form.username_value = base::ASCIIToUTF16(match.username);
      owning_matches.push_back(form);
    }
    std::vector<const PasswordForm*> matches;
    for (const PasswordForm& match : owning_matches)
      matches.push_back(&match);

    std::vector<const PasswordForm*> best_matches;
    const PasswordForm* preferred_match = nullptr;

    std::vector<const PasswordForm*> same_scheme_matches;
    FindBestMatches(matches, PasswordForm::Scheme::kHtml,
                    /*sort_matches_by_date_last_used=*/true,
                    &same_scheme_matches, &best_matches, &preferred_match);

    if (test_case.expected_preferred_match_index == kNotFound) {
      // Case of empty |matches|.
      EXPECT_FALSE(preferred_match);
      EXPECT_TRUE(best_matches.empty());
    } else {
      // Check |preferred_match|.
      EXPECT_EQ(matches[test_case.expected_preferred_match_index],
                preferred_match);
      // Check best matches.
      ASSERT_EQ(test_case.expected_best_matches_indices.size(),
                best_matches.size());

      for (const PasswordForm* match : best_matches) {
        std::string username = base::UTF16ToUTF8(match->username_value);
        ASSERT_NE(test_case.expected_best_matches_indices.end(),
                  test_case.expected_best_matches_indices.find(username));
        size_t expected_index =
            test_case.expected_best_matches_indices.at(username);
        size_t actual_index = std::distance(
            matches.begin(), std::find(matches.begin(), matches.end(), match));
        EXPECT_EQ(expected_index, actual_index);
      }
    }
  }
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsername) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.password_value = base::ASCIIToUTF16("new_password");

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_RejectUnknownUsername) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value = base::ASCIIToUTF16("other_username");

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_FederatedCredential) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.password_value.clear();
  parsed.federation_origin = url::Origin::Create(GURL(kTestFederationURL));

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsernamePSL) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsernamePSLAnotherPassword) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.password_value = base::ASCIIToUTF16("new_password");

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_MatchUsernamePSLNewPasswordKnown) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.new_password_value = parsed.password_value;
  parsed.password_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_MatchUsernamePSLNewPasswordUnknown) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.new_password_value = base::ASCIIToUTF16("new_password");
  parsed.password_value.clear();

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameFindByPassword) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameFindByPasswordPSL) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameCMAPI) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();
  parsed.type = PasswordForm::Type::kApi;

  // In case of the Credential Management API we know for sure that the site
  // meant empty username. Don't try any other heuristics.
  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernamePickFirst) {
  autofill::PasswordForm stored1 = GetTestCredential();
  stored1.username_value = base::ASCIIToUTF16("Adam");
  stored1.password_value = base::ASCIIToUTF16("Adam_password");
  autofill::PasswordForm stored2 = GetTestCredential();
  stored2.username_value = base::ASCIIToUTF16("Ben");
  stored2.password_value = base::ASCIIToUTF16("Ben_password");
  autofill::PasswordForm stored3 = GetTestCredential();
  stored3.username_value = base::ASCIIToUTF16("Cindy");
  stored3.password_value = base::ASCIIToUTF16("Cindy_password");

  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  // The first credential is picked (arbitrarily).
  EXPECT_EQ(&stored3,
            GetMatchForUpdating(parsed, {&stored3, &stored2, &stored1}));
}

TEST(PasswordManagerUtil, MakeNormalizedBlacklistedForm_Android) {
  autofill::PasswordForm blacklisted_credential = MakeNormalizedBlacklistedForm(
      password_manager::PasswordStore::FormDigest(GetTestAndroidCredential()));
  EXPECT_TRUE(blacklisted_credential.blacklisted_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, blacklisted_credential.scheme);
  EXPECT_EQ(kTestAndroidRealm, blacklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestAndroidRealm), blacklisted_credential.origin);
}

TEST(PasswordManagerUtil, MakeNormalizedBlacklistedForm_Html) {
  autofill::PasswordForm blacklisted_credential = MakeNormalizedBlacklistedForm(
      password_manager::PasswordStore::FormDigest(GetTestCredential()));
  EXPECT_TRUE(blacklisted_credential.blacklisted_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, blacklisted_credential.scheme);
  EXPECT_EQ(GURL(kTestURL).GetOrigin().spec(),
            blacklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestURL).GetOrigin(), blacklisted_credential.origin);
}

TEST(PasswordManagerUtil, MakeNormalizedBlacklistedForm_Proxy) {
  autofill::PasswordForm blacklisted_credential = MakeNormalizedBlacklistedForm(
      password_manager::PasswordStore::FormDigest(GetTestProxyCredential()));
  EXPECT_TRUE(blacklisted_credential.blacklisted_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kBasic, blacklisted_credential.scheme);
  EXPECT_EQ(kTestProxySignonRealm, blacklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestProxyOrigin), blacklisted_credential.origin);
}

}  // namespace password_manager_util
