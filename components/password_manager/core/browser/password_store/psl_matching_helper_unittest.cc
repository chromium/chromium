// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

TEST(PSLMatchingUtilsTest, GetMatchResultNormalCredentials) {
  struct TestData {
    const char* form_origin;
    const char* digest_origin;
    MatchResult match_result;
  };

  const TestData cases[] = {
      // Test Exact Matches.
      {"http://facebook.com/p", "http://facebook.com/p1",
       MatchResult::EXACT_MATCH},

      {"http://m.facebook.com/p", "http://m.facebook.com/p1",
       MatchResult::EXACT_MATCH},

      // Scheme mismatch.
      {"http://www.example.com", "https://www.example.com",
       MatchResult::NO_MATCH},

      // Host mismatch.
      {"http://www.example.com", "http://wwwexample.com",
       MatchResult::NO_MATCH},

      {"http://www.example1.com", "http://www.example2.com",
       MatchResult::NO_MATCH},

      // Port mismatch.
      {"http://www.example.com:123/", "http://www.example.com",
       MatchResult::NO_MATCH},

      // TLD mismatch.
      {"http://www.example.org/", "http://www.example.com/",
       MatchResult::NO_MATCH},

      // URLs without registry controlled domains should not match.
      {"http://localhost/", "http://127.0.0.1/", MatchResult::NO_MATCH},

      // Invalid URLs don't match.
      {"http://www.example.com/", "http://", MatchResult::NO_MATCH},

      {"", "http://www.example.com/", MatchResult::NO_MATCH},

      {"http://www.example.com", "bad url", MatchResult::NO_MATCH}};

  for (const TestData& data : cases) {
    PasswordForm form;
    form.url = GURL(data.form_origin);
    form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
    PasswordFormDigest digest(
        PasswordForm::Scheme::kHtml,
        GURL(data.digest_origin).DeprecatedGetOriginAsURL().spec(),
        GURL(data.digest_origin));

    EXPECT_EQ(data.match_result, GetMatchResult(form, digest))
        << "form_origin = " << data.form_origin << ", digest = " << digest;
  }
}

TEST(PSLMatchingUtilsTest, GetMatchResultPSL) {
  struct TestData {
    const char* form_origin;
    const char* digest_origin;
    MatchResult match_result;
  };

  const TestData cases[] = {
      // Test PSL Matches.
      {"http://facebook.com/p1", "http://m.facebook.com/p2",
       MatchResult::PSL_MATCH},

      {"https://www.facebook.com/", "https://m.facebook.com",
       MatchResult::PSL_MATCH},

      // Google sign-in and change password pages are PSL matched.
      {"https://accounts.google.com/", "https://myaccount.google.com/",
       MatchResult::PSL_MATCH},

      // Don't apply PSL matching to other Google domains.
      {"https://accounts.google.com/", "https://maps.google.com/",
       MatchResult::NO_MATCH},
      {"https://subdomain1.google.com/", "https://maps.google.com/",
       MatchResult::NO_MATCH},

      // Scheme mismatch.
      {"http://facebook.com/", "https://m.facebook.com/",
       MatchResult::NO_MATCH},

      {"https://facebook.com/", "http://m.facebook.com/",
       MatchResult::NO_MATCH},

      // Port mismatch.
      {"http://facebook.com/", "https://m.facebook.com:8080/",
       MatchResult::NO_MATCH},

      {"http://facebook.com:8080/", "https://m.facebook.com/",
       MatchResult::NO_MATCH},

      // Port match.
      {"http://facebook.com:8080/p1", "http://m.facebook.com:8080/p2",
       MatchResult::PSL_MATCH},
  };

  for (const TestData& data : cases) {
    PasswordForm form;
    form.url = GURL(data.form_origin);
    form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
    PasswordFormDigest digest(
        PasswordForm::Scheme::kHtml,
        GURL(data.digest_origin).DeprecatedGetOriginAsURL().spec(),
        GURL(data.digest_origin));

    EXPECT_EQ(data.match_result, GetMatchResult(form, digest))
        << "form_origin = " << data.form_origin << ", digest = " << digest;
  }
}

TEST(PSLMatchingUtilsTest, GetMatchResultFederated) {
  struct TestData {
    const char* form_origin;
    const char* form_federation_origin;
    const char* digest_origin;
    MatchResult match_result;
  };

  const TestData cases[] = {
      // Test Federated Matches.
      {"https://example.com/p", "https://google.com", "https://example.com/p2",
       MatchResult::FEDERATED_MATCH},

      // Empty federation providers don't match.
      {"https://example.com/", "", "https://example.com",
       MatchResult::NO_MATCH},

      // Invalid origins don't match.
      {"https://example.com/", "https://google.com", "example.com",
       MatchResult::NO_MATCH},

      {"https://example.com/", "https://google.com", "example",
       MatchResult::NO_MATCH},

      // TLD Mismatch.
      {"https://example.com/", "https://google.com", "https://example.org",
       MatchResult::NO_MATCH},

      // Scheme mismatch.
      {"http://example.com/", "https://google.com", "https://example.com/",
       MatchResult::NO_MATCH},

      {"https://example.com/", "https://google.com", "http://example.com/",
       MatchResult::NO_MATCH},

      // Port mismatch.
      {"https://localhost/", "https://google.com", "http://localhost:8080",
       MatchResult::NO_MATCH},

      {"https://localhost:8080", "https://google.com", "http://localhost",
       MatchResult::NO_MATCH},

      // Port match.
      {"https://localhost:8080/p", "https://google.com",
       "https://localhost:8080/p2", MatchResult::FEDERATED_MATCH},
  };

  for (const TestData& data : cases) {
    PasswordForm form;
    form.url = GURL(data.form_origin);
    form.federation_origin =
        url::SchemeHostPort(GURL(data.form_federation_origin));
    form.signon_realm =
        "federation://" + form.url.host() + "/" + form.federation_origin.host();

    PasswordFormDigest digest(
        PasswordForm::Scheme::kHtml,
        GURL(data.digest_origin).DeprecatedGetOriginAsURL().spec(),
        GURL(data.digest_origin));

    EXPECT_EQ(data.match_result, GetMatchResult(form, digest))
        << "form_origin = " << data.form_origin
        << ", form_federation_origin = " << data.form_federation_origin
        << ", digest = " << digest;
  }
}

TEST(PSLMatchingUtilsTest, GetMatchResultFederatedPSL) {
  struct TestData {
    const char* form_origin;
    const char* form_federation_origin;
    const char* digest_origin;
    MatchResult match_result;
  };

  const TestData cases[] = {
      // Test Federated PSL Matches.
      {"https://sub.example.com/p2", "https://google.com",
       "https://sub.example.com/p1", MatchResult::FEDERATED_MATCH},

      {"https://sub1.example.com/p1", "https://google.com",
       "https://sub2.example.com/p2", MatchResult::FEDERATED_PSL_MATCH},

      {"https://example.com/", "https://google.com", "https://sub.example.com",
       MatchResult::FEDERATED_PSL_MATCH},

      // Federated PSL matches do not apply to HTTP.
      {"https://sub1.example.com/", "https://google.com",
       "http://sub2.example.com", MatchResult::NO_MATCH},

      {"https://example.com/", "https://google.com", "http://sub.example.com",
       MatchResult::NO_MATCH},

      {"http://example.com/", "https://google.com", "https://example.com/",
       MatchResult::NO_MATCH},

      // Federated PSL matches do not apply to Google on HTTP or HTTPS.
      {"https://accounts.google.com/", "https://facebook.com",
       "https://maps.google.com", MatchResult::NO_MATCH},

      {"https://accounts.google.com/facebook.com", "https://facebook.com",
       "http://maps.google.com", MatchResult::NO_MATCH},

      // TLD Mismatch.
      {"https://sub.example.com/google.com", "https://google.com",
       "https://sub.example.org", MatchResult::NO_MATCH},

      // Port mismatch.
      {"https://example.com/", "https://google.com", "https://example.com:8080",
       MatchResult::NO_MATCH},

      {"https://example.com:8080", "https://google.com", "https://example.com",
       MatchResult::NO_MATCH},

      // Port match.
      {"https://sub.example.com:8080/p", "https://google.com",
       "https://sub2.example.com:8080/p2", MatchResult::FEDERATED_PSL_MATCH},
  };

  for (const TestData& data : cases) {
    PasswordForm form;
    form.url = GURL(data.form_origin);
    form.federation_origin =
        url::SchemeHostPort(GURL(data.form_federation_origin));
    form.signon_realm =
        "federation://" + form.url.host() + "/" + form.federation_origin.host();

    PasswordFormDigest digest(
        PasswordForm::Scheme::kHtml,
        GURL(data.digest_origin).DeprecatedGetOriginAsURL().spec(),
        GURL(data.digest_origin));

    EXPECT_EQ(data.match_result, GetMatchResult(form, digest))
        << "form_origin = " << data.form_origin
        << ", form_federation_origin = " << data.form_federation_origin
        << ", digest = " << digest;
  }
}

TEST(PSLMatchingUtilsTest, IsPublicSuffixDomainMatch) {
  struct TestPair {
    const char* url1;
    const char* url2;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"http://facebook.com", "http://facebook.com", true},
      {"http://facebook.com/path", "http://facebook.com/path", true},
      {"http://facebook.com/path1", "http://facebook.com/path2", true},
      {"http://facebook.com", "http://m.facebook.com", true},
      {"http://www.facebook.com", "http://m.facebook.com", true},
      {"http://facebook.com/path", "http://m.facebook.com/path", true},
      {"http://facebook.com/path1", "http://m.facebook.com/path2", true},
      {"http://example.com/has space", "http://example.com/has space", true},
      {"http://www.example.com/%00", "http://www.example.com/%00", true},
      {"http://www.example.com", "http://wwwexample.com", false},
      {"http://www.example.com", "https://www.example.com", false},
      {"http://www.example.com:123", "http://www.example.com", false},
      {"http://www.example.org", "http://www.example.com", false},
      // URLs without registry controlled domains should not match.
      {"http://localhost", "http://127.0.0.1", false},
      // Invalid URLs should not match anything.
      {"http://", "http://", false},
      {"", "", false},
      {"bad url", "bad url", false},
      {"http://www.example.com", "http://", false},
      {"", "http://www.example.com", false},
      {"http://www.example.com", "bad url", false},
      {"http://www.example%00.com", "http://www.example%00.com", false},
      {"federation://example.com/google.com", "https://example.com/", false},
  };

  for (const TestPair& pair : pairs) {
    PasswordForm form1;
    form1.signon_realm = pair.url1;
    PasswordForm form2;
    form2.signon_realm = pair.url2;
    EXPECT_EQ(pair.should_match,
              IsPublicSuffixDomainMatch(form1.signon_realm, form2.signon_realm))
        << "First URL = " << pair.url1 << ", second URL = " << pair.url2;
  }
}

TEST(PSLMatchingUtilsTest, IsFederatedRealm) {
  struct TestPair {
    const char* form_signon_realm;
    const char* origin;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"https://facebook.com", "https://facebook.com", false},
      {"", "", false},
      {"", "https://facebook.com/", false},
      {"https://facebook.com/", "", false},
      {"federation://example.com/google.com", "https://example.com/", true},
      {"federation://example.com/google.com", "http://example.com/", true},
      {"federation://example.com/google.com", "example.com", false},
      {"federation://example.com/", "http://example.com/", false},
      {"federation://example.com/google.com", "example", false},

      {"federation://localhost/google.com", "http://localhost/", true},
      {"federation://localhost/google.com", "http://localhost:8000/", true},
  };

  for (const TestPair& pair : pairs) {
    EXPECT_EQ(pair.should_match,
              IsFederatedRealm(pair.form_signon_realm, GURL(pair.origin)))
        << "form_signon_realm = " << pair.form_signon_realm
        << ", origin = " << pair.origin;
  }
}

TEST(PSLMatchingUtilsTest, IsFederatedPSLMatch) {
  struct TestPair {
    const char* form_signon_realm;
    const char* form_origin;
    const char* origin;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"https://facebook.com", "https://facebook.com", "https://facebook.com",
       false},
      {"", "", "", false},
      {"", "", "https://facebook.com/", false},
      {"https://facebook.com/", "https://facebook.com/", "", false},

      {"federation://example.com/google.com", "https://example.com/p1",
       "https://example.com/p2", true},
      {"federation://example.com/google.com", "https://example.com/",
       "http://example.com/", false},
      {"federation://example.com/google.com", "https://example.com/",
       "example.com", false},
      {"federation://example.com/", "https://example.com/",
       "https://example.com/", false},
      {"federation://example.com/google.com", "https://example.com/", "example",
       false},

      {"federation://sub.example.com/google.com", "https://sub.example.com/p1",
       "https://sub.example.com/p2", true},
      {"federation://sub.example.com/google.com", "https://sub.example.com/p1",
       "https://sub2.example.com/p2", true},
      {"federation://example.com/google.com", "https://example.com/p1",
       "https://sub.example.com/", true},
      {"federation://example.com/google.com", "https://example.com/",
       "http://sub.example.com/", false},
      {"federation://sub.example.com/", "https://sub.example.com/",
       "https://sub.example.com/", false},

      {"federation://localhost/google.com", "http://localhost/",
       "http://localhost/", true},
      {"federation://localhost/google.com", "http://localhost:8000/",
       "http://localhost:8000/", true},
      {"federation://localhost/google.com", "http://localhost:8000/",
       "http://localhost/", false},
      {"federation://localhost/google.com", "http://localhost/",
       "http://localhost:8000/", false},
  };

  for (const TestPair& pair : pairs) {
    EXPECT_EQ(pair.should_match,
              IsFederatedPSLMatch(pair.form_signon_realm,
                                  GURL(pair.form_origin), GURL(pair.origin)))
        << "form_signon_realm = " << pair.form_signon_realm
        << "form_origin = " << pair.form_origin << ", origin = " << pair.origin;
  }
}

}  // namespace

}  // namespace password_manager
