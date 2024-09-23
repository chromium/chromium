// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"
#include "components/url_formatter/spoof_checks/top_domains/test_top_bucket_domains.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {
#include "components/url_formatter/spoof_checks/common_words/common_words_test-inc.cc"
#include "components/url_formatter/spoof_checks/top_domains/test_domains-trie-inc.cc"
}  // namespace test

using lookalikes::ComboSquattingParams;
using lookalikes::ComboSquattingType;
using lookalikes::DomainInfo;
using lookalikes::GetDomainInfo;
using lookalikes::IsHeuristicEnabledForHostname;
using lookalikes::LookalikeUrlMatchType;
using lookalikes::TargetEmbeddingType;
using version_info::Channel;

namespace {
// Tests lists for Combo Squatting. Some of these entries are intended to test
// for various edge cases and aren't realistic for production.
constexpr std::pair<const char*, const char*> kBrandNames[] = {
    {"google", "google"},
    {"youtube", "youtube"},
    {"sample", "sarnple"},
    {"example", "exarnple"},
    {"vices", "vices"}};
const char* const kPopularKeywords[] = {
    "online", "login", "account", "arnple", "services", "test", "security"};
const ComboSquattingParams kComboSquattingParams{
    kBrandNames, std::size(kBrandNames), kPopularKeywords,
    std::size(kPopularKeywords)};

}  // namespace

std::string TargetEmbeddingTypeToString(TargetEmbeddingType type) {
  switch (type) {
    case TargetEmbeddingType::kNone:
      return "kNone";
    case TargetEmbeddingType::kInterstitial:
      return "kInterstitial";
    case TargetEmbeddingType::kSafetyTip:
      return "kSafetyTip";
  }
  NOTREACHED_IN_MIGRATION();
}

// These tests do not use the production top domain list. This is to avoid
// having to adjust the tests when the top domain list is updated. Instead,
// these tests use the data in `test_domains.list` and `common_words_test.gpref`
// files.
class LookalikeUrlUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    // Use test top domain lists instead of the actual list.
    url_formatter::IDNSpoofChecker::HuffmanTrieParams trie_params{
        test::kTopDomainsHuffmanTree, sizeof(test::kTopDomainsHuffmanTree),
        test::kTopDomainsTrie, test::kTopDomainsTrieBits,
        test::kTopDomainsRootPosition};
    url_formatter::IDNSpoofChecker::SetTrieParamsForTesting(trie_params);

    // Use test top bucket domain skeletons instead of the actual list.
    lookalikes::TopBucketDomainsParams top_bucket_params{
        test_top_bucket_domains::kTopBucketEditDistanceSkeletons,
        test_top_bucket_domains::kNumTopBucketEditDistanceSkeletons};
    lookalikes::SetTopBucketDomainsParamsForTesting(top_bucket_params);

    url_formatter::common_words::SetCommonWordDAFSAForTesting(test::kDafsa);
  }

  void TearDown() override {
    url_formatter::common_words::ResetCommonWordDAFSAForTesting();
    lookalikes::ResetTopBucketDomainsParamsForTesting();
    url_formatter::IDNSpoofChecker::RestoreTrieParamsForTesting();
  }
};

TEST_F(LookalikeUrlUtilTest, IsEditDistanceAtMostOne) {
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
    bool result = lookalikes::IsEditDistanceAtMostOne(
        base::WideToUTF16(test_case.domain),
        base::WideToUTF16(test_case.top_domain));
    EXPECT_EQ(test_case.expected, result)
        << "when comparing " << test_case.domain << " with "
        << test_case.top_domain;
  }
}

TEST_F(LookalikeUrlUtilTest, EditDistanceExcludesCommonFalsePositives) {
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

      // We permit matches that only differ due to a single "-".
      {"-abcde.com", "abcde.com", true},
      {"ab-cde.com", "abcde.com", true},
      {"abcde-.com", "abcde.com", true},
      {"abcde.com", "-abcde.com", true},
      {"abcde.com", "ab-cde.com", true},
      {"abcde.com", "abcde-.com", true},
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

TEST_F(LookalikeUrlUtilTest, CharacterSwapExcludesCommonFalsePositives) {
  const struct TestCase {
    const char* domain;
    const char* top_domain;
    bool is_likely_false_positive;
  } kTestCases[] = {
      {"abcde.com", "abced.com", false},
      // Only differs by registry:
      {"abcde.sr", "abcde.rs", true},
  };
  for (const TestCase& test_case : kTestCases) {
    auto navigated =
        GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                           url::kStandardSchemeSeparator + test_case.domain));
    auto matched = GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                                      url::kStandardSchemeSeparator +
                                      test_case.top_domain));
    bool result = IsLikelyCharacterSwapFalsePositive(navigated, matched);
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

TEST_F(LookalikeUrlUtilTest, ShouldBlockBySpoofCheckResult) {
  EXPECT_FALSE(ShouldBlockBySpoofCheckResult(
      GetDomainInfo(GURL("https://example.com"))));
  // ASCII short eTLD+1:
  EXPECT_FALSE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://e.com"))));
  EXPECT_FALSE(ShouldBlockBySpoofCheckResult(
      GetDomainInfo(GURL("https://subdomain.e.com"))));
  // Unicode single character e2LD:
  EXPECT_FALSE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://τ.com"))));
  EXPECT_FALSE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://test.τ.com"))));
  // Unicode single character e2LD with a unicode registry.
  EXPECT_FALSE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://τ.рф"))));
  EXPECT_FALSE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://test.τ.рф"))));
  // Non-unique hostname:
  EXPECT_FALSE(ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://τ"))));

  // Multi character e2LD with disallowed characters:
  EXPECT_TRUE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://ττ.com"))));
  EXPECT_TRUE(ShouldBlockBySpoofCheckResult(
      GetDomainInfo(GURL("https://test.ττ.com"))));
  EXPECT_TRUE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://ττ.рф"))));
  EXPECT_TRUE(
      ShouldBlockBySpoofCheckResult(GetDomainInfo(GURL("https://test.ττ.рф"))));
}

TEST_F(LookalikeUrlUtilTest, TargetEmbeddingTest) {
  const std::vector<DomainInfo> kEngagedSites = {
      GetDomainInfo(GURL("https://highengagement.com")),
      GetDomainInfo(GURL("https://highengagement.inthesubdomain.com")),
      GetDomainInfo(GURL("https://highengagement.co.uk")),
      GetDomainInfo(GURL("https://subdomain.highengagement.com")),
      GetDomainInfo(GURL("https://www.highengagementwithwww.com")),
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
      // Common words (like 'jobs' are included in the big common word list.
      // Ensure that the supplemental kCommonWords list is also checked.
      {"foo.hoteles.com-foo.com", "", TargetEmbeddingType::kNone},

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

      // Cross-TLD matches should not trigger, even when they're embedding
      // another domain, even when using a de-facto public eTLD.
      {"google.com.mx", "", TargetEmbeddingType::kNone},  // public
      {"google.com.de", "", TargetEmbeddingType::kNone},  // de-facto public

      // Engaged sites should trigger as specifically as possible, and should
      // trigger preferentially to top sites when possible.
      {"foo.highengagement.com.foo.com", "highengagement.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.subdomain.highengagement.com.foo.com",
       "subdomain.highengagement.com", TargetEmbeddingType::kInterstitial},
      {"foo.subdomain.google.com.foo.com", "subdomain.google.com",
       TargetEmbeddingType::kInterstitial},

      // Skeleton matching should work against engaged sites at a eTLD+1 level,
      {"highengagement.inthesubdomain.com-foo.com",
       "highengagement.inthesubdomain.com", TargetEmbeddingType::kInterstitial},
      // but only if the bare eTLD+1, or www.[eTLD+1] has been engaged.
      {"subdomain.highéngagement.com-foo.com", "highengagement.com",
       TargetEmbeddingType::kInterstitial},
      {"subdomain.highéngagementwithwww.com-foo.com",
       "highengagementwithwww.com", TargetEmbeddingType::kInterstitial},
      {"other.inthésubdomain.com-foo.com", "", TargetEmbeddingType::kNone},
      // Ideally, we'd be able to combine subdomains and skeleton matching, but
      // our current algorithm can't detect that precisely.
      {"highengagement.inthésubdomain.com-foo.com", "",
       TargetEmbeddingType::kNone},

      // Domains should be allowed to embed themselves.
      {"highengagement.com.highengagement.com", "", TargetEmbeddingType::kNone},
      {"subdomain.highengagement.com.highengagement.com", "",
       TargetEmbeddingType::kNone},
      {"nothighengagement.highengagement.com.highengagement.com", "",
       TargetEmbeddingType::kNone},
      {"google.com.google.com", "", TargetEmbeddingType::kNone},
      {"www.google.com.google.com", "", TargetEmbeddingType::kNone},

      // Detect embeddings at the end of the domain, too, but as a Safety Tip.
      {"www-google.com", "google.com", TargetEmbeddingType::kSafetyTip},
      {"www-highengagement.com", "highengagement.com",
       TargetEmbeddingType::kSafetyTip},
      {"subdomain-highengagement.com", "subdomain.highengagement.com",
       TargetEmbeddingType::kSafetyTip},
      // If the match duplicates the TLD, it's not quite tail-embedding.
      {"google-com.com", "google.com", TargetEmbeddingType::kInterstitial},
      // If there are multiple options, it should choose the more severe one.
      {"google-com.google-com.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"subdomain.google-com.google-com.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"google.com-google.com-google.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // Ignore end-of-domain embeddings when they're also cross-TLD matches.
      {"google.com.mx", "", TargetEmbeddingType::kNone},

      // For a small set of high-value domains that are also common words (see
      // kDomainsPermittedInEndEmbeddings), we block all embeddings except those
      // at the very end of the domain (e.g. foo-{domain.com}). Ensure this
      // works for domains on the list, but not for others.
      {"office.com-foo.com", "office.com", TargetEmbeddingType::kInterstitial},
      {"example-office.com", "", TargetEmbeddingType::kNone},
      {"example-google.com", "google.com", TargetEmbeddingType::kSafetyTip},
  };

  lookalikes::InitializeBlankLookalikeAllowlistForTesting();
  auto* config_proto = lookalikes::GetSafetyTipsRemoteConfigProto();

  for (auto& test_case : kTestCases) {
    std::string safe_hostname;
    TargetEmbeddingType embedding_type = GetTargetEmbeddingType(
        test_case.hostname, kEngagedSites,
        base::BindRepeating(&IsGoogleScholar), config_proto, &safe_hostname);
    if (test_case.expected_type != TargetEmbeddingType::kNone) {
      EXPECT_EQ(safe_hostname, test_case.expected_safe_host)
          << test_case.hostname << " should trigger on "
          << test_case.expected_safe_host << ", but "
          << (safe_hostname.empty() ? "it didn't trigger at all."
                                    : "triggered on " + safe_hostname);
      EXPECT_EQ(embedding_type, test_case.expected_type)
          << test_case.hostname << " should trigger "
          << TargetEmbeddingTypeToString(test_case.expected_type) << " against "
          << test_case.expected_safe_host << " but it returned "
          << TargetEmbeddingTypeToString(embedding_type);
    } else {
      EXPECT_EQ(embedding_type, TargetEmbeddingType::kNone)
          << test_case.hostname << " unexpectedly triggered "
          << TargetEmbeddingTypeToString(embedding_type) << " against "
          << safe_hostname;
    }
  }
}

TEST_F(LookalikeUrlUtilTest, TargetEmbeddingIgnoresComponentWordlist) {
  const std::vector<DomainInfo> kEngagedSites = {
      GetDomainInfo(GURL("https://commonword.com")),
      GetDomainInfo(GURL("https://uncommonword.com")),
  };

  lookalikes::SetSafetyTipAllowlistPatterns({}, {}, {"commonword"});
  auto* config_proto = lookalikes::GetSafetyTipsRemoteConfigProto();
  TargetEmbeddingType embedding_type;
  std::string safe_hostname;

  // Engaged sites using uncommon words are still blocked.
  embedding_type = GetTargetEmbeddingType(
      "uncommonword.com.evil.com", kEngagedSites,
      base::BindRepeating(&IsGoogleScholar), config_proto, &safe_hostname);
  EXPECT_EQ(embedding_type, TargetEmbeddingType::kInterstitial);

  // But engaged sites using common words are not blocked.
  embedding_type = GetTargetEmbeddingType(
      "commonword.com.evil.com", kEngagedSites,
      base::BindRepeating(&IsGoogleScholar), config_proto, &safe_hostname);
  EXPECT_EQ(embedding_type, TargetEmbeddingType::kNone);
}

TEST_F(LookalikeUrlUtilTest, GetETLDPlusOneHandlesSpecialRegistries) {
  const struct GetETLDPlusOneTestCase {
    const std::string hostname;
    const std::string expected_etldp1;
  } kTestCases[] = {
      // Trivial test cases for public registries.
      {"google.com", "google.com"},
      {"www.google.com", "google.com"},
      {"www.google.co.uk", "google.co.uk"},

      // .com.de is a de-facto public registry.
      {"www.google.com.de", "google.com.de"},
      // Regression test for crbug.com/351775838:
      {"com.de", ""},

      // .cloud.goog is a private registry.
      {"www.example.cloud.goog", "cloud.goog"},
      {"cloud.goog", "cloud.goog"},
  };

  for (auto& test_case : kTestCases) {
    EXPECT_EQ(lookalikes::GetETLDPlusOne(test_case.hostname),
              test_case.expected_etldp1);
  }
}

// Tests for the character swap heuristic.
TEST_F(LookalikeUrlUtilTest, HasOneCharacterSwap) {
  const struct TestCase {
    const wchar_t* str1;
    const wchar_t* str2;
    bool expected;
  } kTestCases[] = {{L"", L"", false},
                    {L"", L"a", false},
                    {L"", L"ab", false},
                    {L"a", L"ab", false},
                    {L"a", L"ba", false},
                    {L"abc.com", L"abc.com", false},
                    {L"abc.com", L"abcd.com", false},
                    {L"domain.com", L"nomaid.com", false},
                    // Two swaps (ab to ba, ba to ab):
                    {L"abba", L"baab", false},

                    {L"ab", L"ba", true},
                    {L"abba", L"baba", true},

                    {L"abaaa", L"baaaa", true},
                    {L"abcaa", L"bacaa", true},

                    {L"aaaab", L"aaaba", true},
                    {L"aacab", L"aacba", true},

                    {L"aabaa", L"abaaa", true},
                    {L"aabcc", L"abacc", true},

                    {L"aabaa", L"aaaba", true},
                    {L"ccbaa", L"ccaba", true},

                    {L"domain.com", L"doamin.com", true},
                    {L"gmail.com", L"gmailc.om", true},
                    {L"gmailc.om", L"gmail.com", true}};
  for (const TestCase& test_case : kTestCases) {
    bool result = lookalikes::HasOneCharacterSwap(
        base::WideToUTF16(test_case.str1), base::WideToUTF16(test_case.str2));
    EXPECT_EQ(test_case.expected, result)
        << "when comparing " << test_case.str1 << " with " << test_case.str2;
  }
}

TEST_F(LookalikeUrlUtilTest, GetSuggestedURL) {
  const struct TestCase {
    const LookalikeUrlMatchType match_type;
    const GURL navigated_url;
    const std::string matched_hostname;
    const GURL expected_suggested_url;
  } kTestCases[] = {
      // Certain heuristics such as top domain matches should use https for
      // the suggested URL.
      {LookalikeUrlMatchType::kSkeletonMatchTop500,
       GURL("http://docs.googlé.com"), "google.com",
       GURL("https://google.com")},
      // But not for non-default ports:
      {LookalikeUrlMatchType::kSkeletonMatchTop500,
       GURL("http://docs.googlé.com:8080"), "google.com",
       GURL("http://google.com:8080")},
      // Site engagement should use http for the suggested URL.
      {LookalikeUrlMatchType::kSkeletonMatchSiteEngagement,
       GURL("http://docs.googlé.com"), "google.com", GURL("http://google.com")},

      // Same tests with the matched hostname having a subdomain.
      {LookalikeUrlMatchType::kSkeletonMatchTop500,
       GURL("http://docs.googlé.com"), "docs.google.com",
       GURL("https://google.com")},
      {LookalikeUrlMatchType::kSkeletonMatchTop500,
       GURL("http://docs.googlé.com:8080"), "docs.google.com",
       GURL("http://google.com:8080")},
      {LookalikeUrlMatchType::kSkeletonMatchSiteEngagement,
       GURL("http://docs.googlé.com"), "docs.google.com",
       GURL("http://google.com")},

      // Same tests with neither the matched hostname or navigated domain having
      // a subdomain.
      {LookalikeUrlMatchType::kSkeletonMatchTop500, GURL("http://googlé.com"),
       "docs.google.com", GURL("https://google.com")},
      {LookalikeUrlMatchType::kSkeletonMatchTop500,
       GURL("http://googlé.com:8080"), "docs.google.com",
       GURL("http://google.com:8080")},
      {LookalikeUrlMatchType::kSkeletonMatchSiteEngagement,
       GURL("http://googlé.com"), "docs.google.com", GURL("http://google.com")},
  };

  for (const TestCase& test_case : kTestCases) {
    GURL suggested_url =
        GetSuggestedURL(test_case.match_type, test_case.navigated_url,
                        test_case.matched_hostname);
    EXPECT_EQ(test_case.expected_suggested_url, suggested_url);
  }
}

TEST_F(LookalikeUrlUtilTest, IsHeuristicEnabledForHostname) {
  reputation::SafetyTipsConfig proto;
  reputation::HeuristicLaunchConfig* config = proto.add_launch_config();
  config->set_heuristic(reputation::HeuristicLaunchConfig::
                            HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES);

  // Minimum rollout percentages to enable a heuristic on each site on Stable
  // channel:
  // example1.com: 79%
  // example2.com: 16%
  // example3.com: 36%

  // Slowly ramp up the launch and cover more sites on Stable channel.
  config->set_launch_percentage(0);
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example1.com", Channel::STABLE));
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example2.com", Channel::STABLE));
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example3.com", Channel::STABLE));

  config->set_launch_percentage(25);
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example1.com", Channel::STABLE));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example2.com", Channel::STABLE));
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example3.com", Channel::STABLE));

  config->set_launch_percentage(50);
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example1.com", Channel::STABLE));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example2.com", Channel::STABLE));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example3.com", Channel::STABLE));

  config->set_launch_percentage(100);
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example1.com", Channel::STABLE));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example2.com", Channel::STABLE));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example3.com", Channel::STABLE));

  // On Beta, launch is always at 50%.
  config->set_launch_percentage(0);
  EXPECT_FALSE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example1.com", Channel::BETA));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example2.com", Channel::BETA));
  EXPECT_TRUE(IsHeuristicEnabledForHostname(
      &proto,
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_ENGAGED_SITES,
      "example3.com", Channel::BETA));
}

class ComboSquattingTest : public testing::Test {
 protected:
  void SetUp() override {
    lookalikes::SetComboSquattingParamsForTesting(kComboSquattingParams);
  }
  void TearDown() override {
    lookalikes::ResetComboSquattingParamsForTesting();
  }
};

// Test for Combo Squatting check of domains.
TEST_F(ComboSquattingTest, IsComboSquatting) {
  const std::vector<DomainInfo> kEngagedSites = {
      // An engaged site which is not in the hard coded brand names.
      GetDomainInfo(GURL("https://engagedsite.com")),
      // An engaged site which is duplicate with a hard coded brand name.
      GetDomainInfo(GURL("https://subdomain.google.com")),
      // An engaged site with length less than threshold (4) for
      // consideration.
      GetDomainInfo(GURL("https://len.com")),
      // An engaged site with a registry other than com.
      GetDomainInfo(GURL("https://testcombo.org")),
      // Test case for overlapping brand name and keyword (highs +
      // security = highsecurity). Single letter overlap.
      GetDomainInfo(GURL("https://highs.com")),
  };
  const struct TestCase {
    const char* domain;
    const char* expected_suggested_domain;
    const ComboSquattingType expected_type;
  } kTestCases[] = {
      // Not Combo Squatting (CSQ).
      {"google.com", "", ComboSquattingType::kNone},
      {"youtube.ca", "", ComboSquattingType::kNone},

      // Not CSQ, contains subdomains.
      {"login.google.com", "", ComboSquattingType::kNone},

      // Not CSQ, non registrable domains.
      {"google-login.test", "", ComboSquattingType::kNone},

      // CSQ with "-".
      {"google-online.com", "google.com", ComboSquattingType::kHardCoded},

      // CSQ with more than one keyword (login, online) with "-".
      {"google-login-online.com", "google.com", ComboSquattingType::kHardCoded},

      // CSQ with one keyword (online) and one random word (one) with "-".
      {"one-sample-online.com", "sample.com", ComboSquattingType::kHardCoded},

      // Not CSQ, with a keyword (test) as TLD.
      {"www.example.test", "", ComboSquattingType::kNone},

      // CSQ with more than one brand (google, youtube) with "-".
      {"google-youtube-account.com", "google.com",
       ComboSquattingType::kHardCoded},

      // CSQ without separator.
      {"loginsample.com", "sample.com", ComboSquattingType::kHardCoded},

      // Not CSQ with a keyword (ample) inside brand name (sample).
      {"sample.com", "", ComboSquattingType::kNone},

      // Current version of the heuristic cannot flag this kind of CSQ
      // with a keyword (ample) inside brand name (sample) and as an added
      // keyword to the domain.
      {"sample-ample.com", "", ComboSquattingType::kNone},

      // CSQ with more than one keyword (account, online) without separator.
      {"accountexampleonline.com", "example.com",
       ComboSquattingType::kHardCoded},

      // CSQ with one keyword (login) and one random word (one) without "-".
      {"oneyoutubelogin.com", "youtube.com", ComboSquattingType::kHardCoded},

      // Not CSQ, google is a public TLD.
      {"online.google", "", ComboSquattingType::kNone},

      // Not CSQ, brand name (vice) is part of keyword (service).
      {"keyservices.com", "", ComboSquattingType::kNone},

      // CSQ, brand name (engagedsite) is from engaged sites list.
      {"engagedsite-login.com", "engagedsite.com",
       ComboSquattingType::kSiteEngagement},

      // Not CSQ, brand name (len) is from engaged sites list but it is short.
      {"len-online.com", "", ComboSquattingType::kNone},

      // CSQ, brand name (googlé) is one of the hard coded brand names and has
      // IDN spoofing as well.
      {"googlé-login.com", "google.com", ComboSquattingType::kHardCoded},

      // CSQ, brand name (engagedsité) is one of the brand names from engaged
      // sites and has IDN spoofing as well.
      {"engagedsité-online.com", "engagedsite.com",
       ComboSquattingType::kSiteEngagement},

      // CSQ, keyword (lógin) has IDN spoofing.
      {"google-lógin.com", "google.com", ComboSquattingType::kHardCoded},

      // CSQ, CSQ with more than one brand (googlé, youtubé) with "-" and IDN
      // spoofing.
      {"googlé-youtubé-account.com", "google.com",
       ComboSquattingType::kHardCoded},

      // Not CSQ.
      {"ónline.googlé", "", ComboSquattingType::kNone},

      // Not CSQ, it has IDN spoofing but brand name (vicé) is part of keyword
      // (servicé).
      {"keyservicés.com", "", ComboSquattingType::kNone},

      // CSQ without separator and with IDN spoofing in the keyword.
      {"lóginsample.com", "sample.com", ComboSquattingType::kHardCoded},

      // CSQ without separator and with IDN spoofing in the brand name.
      {"loginsamplé.com", "sample.com", ComboSquattingType::kHardCoded},

      // Not CSQ, skeleton of brand name (lén) is from engaged sites list but it
      // is short.
      {"lén-online.com", "", ComboSquattingType::kNone},

      // CSQ when domain and registry are in top domains.
      {"google-login.co.kr", "google.co.kr", ComboSquattingType::kHardCoded},

      // CSQ when brand name is in hard coded brand names, but domain and
      // registry are not in top domains.
      {"google-login.co.ir", "google.com", ComboSquattingType::kHardCoded},

      // CSQ when domain and registry are in engaged sites, with registry other
      // than com.
      {"testcomboonline.org", "testcombo.org",
       ComboSquattingType::kSiteEngagement},

      // If the brand name (highsec) and keyword (security) overlap, ignore.
      {"highsecurity.com", "", ComboSquattingType::kNone},
  };
  for (const TestCase& test_case : kTestCases) {
    auto navigated =
        GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                           url::kStandardSchemeSeparator + test_case.domain));
    std::string matched_domain;
    ComboSquattingType type =
        GetComboSquattingType(navigated, kEngagedSites, &matched_domain);
    EXPECT_EQ(std::string(test_case.expected_suggested_domain), matched_domain);
    EXPECT_EQ(test_case.expected_type, type);
  }
}
