// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/lookalikes/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LookalikeUrlUtilTest, IsEditDistanceAtMostOne) {
  const struct TestCase {
    const wchar_t* domain;
    const wchar_t* top_domain;
    bool expected;
  } kTestCases[] = {
      {L"", L"", true},
      {L"a", L"a", true},
      {L"a", L"", true},
      {L"", L"a", true},

      {L"", L"ab", false},
      {L"ab", L"", false},

      {L"ab", L"a", true},
      {L"a", L"ab", true},
      {L"ab", L"b", true},
      {L"b", L"ab", true},
      {L"ab", L"ab", true},

      {L"", L"ab", false},
      {L"ab", L"", false},
      {L"a", L"abc", false},
      {L"abc", L"a", false},

      {L"aba", L"ab", true},
      {L"ba", L"aba", true},
      {L"abc", L"ac", true},
      {L"ac", L"abc", true},

      // Same length.
      {L"xbc", L"ybc", true},
      {L"axc", L"ayc", true},
      {L"abx", L"aby", true},

      // Should also work for non-ASCII.
      {L"é", L"", true},
      {L"", L"é", true},
      {L"tést", L"test", true},
      {L"test", L"tést", true},
      {L"tés", L"test", false},
      {L"test", L"tés", false},

      // Real world test cases.
      {L"google.com", L"gooogle.com", true},
      {L"gogle.com", L"google.com", true},
      {L"googlé.com", L"google.com", true},
      {L"google.com", L"googlé.com", true},
      // Different by two characters.
      {L"google.com", L"goooglé.com", false},
  };
  for (const TestCase& test_case : kTestCases) {
    bool result =
        IsEditDistanceAtMostOne(base::WideToUTF16(test_case.domain),
                                base::WideToUTF16(test_case.top_domain));
    EXPECT_EQ(test_case.expected, result)
        << "when comparing " << test_case.domain << " with "
        << test_case.top_domain;
  }
}

TEST(LookalikeUrlUtilTest, EditDistanceExcludesCommonFalsePositives) {
  const struct TestCase {
    const char* domain;
    const char* top_domain;
    bool is_likely_false_positive;
  } kTestCases[] = {
      // Most edit distance instances are not likely false positives.
      {"abcxd.com", "abcyd.com", false},   // Substitution
      {"abcxd.com", "abcxxd.com", false},  // Deletion
      {"abcxxd.com", "abcxd.com", false},  // Insertion

      // But we permit cases where the only difference is in the tld.
      {"abcde.com", "abcde.net", true},

      // We also permit matches that are only due to a numeric suffix,
      {"abcd1.com", "abcd2.com", true},    // Substitution
      {"abcde.com", "abcde1.com", true},   // Numeric deletion
      {"abcde1.com", "abcde.com", true},   // Numeric insertion
      {"abcd11.com", "abcd21.com", true},  // Not-final-digit substitution
      {"a.abcd1.com", "abcd2.com", true},  // Only relevant for eTLD+1.
      // ...and that change must be due to the numeric suffix.
      {"abcx1.com", "abcy1.com", false},   // Substitution before suffix
      {"abcd1.com", "abcde1.com", false},  // Deletion before suffix
      {"abcde1.com", "abcd1.com", false},  // Insertion before suffix
      {"abcdx.com", "abcdy.com", false},   // Non-numeric substitution at end

      // We also permit matches that are only due to a first-character change,
      {"xabcd.com", "yabcd.com", true},     // Substitution
      {"xabcde.com", "abcde.com", true},    // Insertion
      {"abcde.com", "xabcde.com", true},    // Deletion
      {"a.abcde.com", "xabcde.com", true},  // For eTLD+1
      // ...so long as that change is only on the first character, not later.
      {"abcde.com", "axbcde.com", false},   // Deletion
      {"axbcde.com", "abcde.com", false},   // Insertion
      {"axbcde.com", "aybcde.com", false},  // Substitution
  };
  for (const TestCase& test_case : kTestCases) {
    auto navigated =
        GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                           url::kStandardSchemeSeparator + test_case.domain));
    auto matched = GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                                      url::kStandardSchemeSeparator +
                                      test_case.top_domain));
    bool result = IsLikelyEditDistanceFalsePositive(navigated, matched);
    EXPECT_EQ(test_case.is_likely_false_positive, result)
        << "when comparing " << test_case.domain << " with "
        << test_case.top_domain;
  }
}

bool IsGoogleScholar(const std::string& hostname) {
  return hostname == "scholar.google.com";
}

struct TargetEmbeddingHeuristicTestCase {
  const std::string hostname;
  // Empty when there is no match.
  const std::string expected_safe_host;
  const TargetEmbeddingType expected_type;
};

TEST(LookalikeUrlUtilTest, TargetEmbeddingTest) {
  const std::vector<DomainInfo> kEngagedSites = {
      GetDomainInfo(GURL("https://highengagement.com")),
      GetDomainInfo(GURL("https://highengagement.co.uk")),
      GetDomainInfo(GURL("https://subdomain.highengagement.com")),
      GetDomainInfo(GURL("https://subdomain.google.com")),
  };
  const std::vector<TargetEmbeddingHeuristicTestCase> kTestCases = {
      // The length of the url should not affect the outcome.
      {"this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test-google.com-login.com",
       "google.com", TargetEmbeddingType::kInterstitial},
      {"google-com-this-is-a-very-long-url-but-it-should-not-affect-"
       "the-outcome-of-this-target-embedding-test-login.com",
       "google.com", TargetEmbeddingType::kInterstitial},
      {"this-is-a-very-long-url-but-it-should-not-affect-google-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       "", TargetEmbeddingType::kNone},
      {"google-this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       "", TargetEmbeddingType::kNone},

      // We need exact skeleton match for our domain so exclude edit-distance
      // matches.
      {"goog0le.com-login.com", "", TargetEmbeddingType::kNone},

      // Unicode characters should be handled
      {"googlé.com-login.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo-googlé.com-bar.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // The basic states
      {"google.com.foo.com", "google.com", TargetEmbeddingType::kInterstitial},
      // - before the domain name should be ignored.
      {"foo-google.com-bar.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // The embedded target's TLD doesn't necessarily need to be followed by a
      // '-' and could be a subdomain by itself.
      {"foo-google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"a.b.c.d.e.f.g.h.foo-google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"a.b.c.d.e.f.g.h.google.com-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"1.2.3.4.5.6.google.com-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // Target domain could be in the middle of subdomains.
      {"foo.google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // The target domain and its tld should be next to each other.
      {"foo-google.l.com-foo.com", "", TargetEmbeddingType::kNone},
      // Target domain might be separated with a dash instead of dot.
      {"foo.google-com-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // Allowlisted domains should not trigger heuristic.
      {"scholar.google.com.foo.com", "", TargetEmbeddingType::kNone},
      {"scholar.google.com-google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"google.com-scholar.google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.scholar.google.com.foo.com", "", TargetEmbeddingType::kNone},
      {"scholar.foo.google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // e2LDs should be longer than 3 characters.
      {"hp.com-foo.com", "", TargetEmbeddingType::kNone},

      // Targets with common words as e2LD are not considered embedded targets
      // either for all TLDs or another-TLD matching.
      {"foo.jobs.com-foo.com", "", TargetEmbeddingType::kNone},
      {"foo.office.com-foo.com", "office.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.jobs.org-foo.com", "", TargetEmbeddingType::kNone},
      {"foo.office.org-foo.com", "", TargetEmbeddingType::kNone},

      // Targets could be embedded without their dots and dashes.
      {"googlecom-foo.com", "google.com", TargetEmbeddingType::kInterstitial},
      {"foo.googlecom-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // But should not be detected if they're using a common word. weather.com
      // is on the top domain list, but 'weather' is a common word.
      {"weathercom-foo.com", "", TargetEmbeddingType::kNone},
      // And should also not be detected if they're too short. vk.com is on the
      // top domain list, but is shorter than kMinE2LDLengthForTargetEmbedding.
      {"vkcom-foo.com", "", TargetEmbeddingType::kNone},

      // Ensure legitimate domains don't trigger.
      {"foo.google.com", "", TargetEmbeddingType::kNone},
      {"foo.bar.google.com", "", TargetEmbeddingType::kNone},
      {"google.com", "", TargetEmbeddingType::kNone},
      {"google.co.uk", "", TargetEmbeddingType::kNone},
      {"google.randomreg-login.com", "", TargetEmbeddingType::kNone},
      {"com.foo.com", "", TargetEmbeddingType::kNone},

      // Multipart eTLDs should work.
      {"foo.google.co.uk.foo.com", "google.co.uk",
       TargetEmbeddingType::kInterstitial},
      {"foo.highengagement-co-uk.foo.com", "highengagement.co.uk",
       TargetEmbeddingType::kInterstitial},

      // Engaged sites should trigger as specifically as possible, and should
      // trigger preferentially to top sites when possible.
      {"foo.highengagement.com.foo.com", "highengagement.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.subdomain.highengagement.com.foo.com",
       "subdomain.highengagement.com", TargetEmbeddingType::kInterstitial},
      {"foo.subdomain.google.com.foo.com", "subdomain.google.com",
       TargetEmbeddingType::kInterstitial},

      // Skeleton matching should work against engaged sites at the eTLD level.
      {"subdomain.highéngagement.com-foo.com", "highengagement.com",
       TargetEmbeddingType::kInterstitial},

      // Domains should be allowed to embed themselves.
      {"highengagement.com.highengagement.com", "", TargetEmbeddingType::kNone},
      {"subdomain.highengagement.com.highengagement.com", "",
       TargetEmbeddingType::kNone},
      {"nothighengagement.highengagement.com.highengagement.com", "",
       TargetEmbeddingType::kNone},
      {"google.com.google.com", "", TargetEmbeddingType::kNone},
      {"www.google.com.google.com", "", TargetEmbeddingType::kNone},

      // Detect embeddings at the end of the domain, too.
      {"www-google.com", "google.com", TargetEmbeddingType::kInterstitial},
      {"www-highengagement.com", "highengagement.com",
       TargetEmbeddingType::kInterstitial},
      {"subdomain-highengagement.com", "subdomain.highengagement.com",
       TargetEmbeddingType::kInterstitial},
      {"google-com.google-com.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"subdomain.google-com.google-com.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"google.com-google.com-google.com", "google.com",
       TargetEmbeddingType::kInterstitial},
  };

  for (auto& test_case : kTestCases) {
    std::string safe_hostname;
    TargetEmbeddingType embedding_type = GetTargetEmbeddingType(
        test_case.hostname, kEngagedSites,
        base::BindRepeating(&IsGoogleScholar), &safe_hostname);
    if (test_case.expected_type != TargetEmbeddingType::kNone) {
      EXPECT_EQ(safe_hostname, test_case.expected_safe_host)
          << test_case.hostname << " should trigger on "
          << test_case.expected_safe_host << ", but "
          << (safe_hostname.empty() ? "it didn't trigger at all."
                                    : "triggered on " + safe_hostname);
      EXPECT_EQ(embedding_type, test_case.expected_type);
    } else {
      EXPECT_EQ(embedding_type, TargetEmbeddingType::kNone)
          << test_case.hostname << " unexpectedly triggered on "
          << safe_hostname;
    }
  }
}
