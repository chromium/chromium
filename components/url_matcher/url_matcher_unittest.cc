// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/url_matcher/url_matcher.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::MatcherStringPattern;

namespace url_matcher {

//
// URLMatcherCondition
//

TEST(URLMatcherConditionTest, Constructors) {
  MatcherStringPattern pattern("example.com", 1);
  URLMatcherCondition m1(URLMatcherCondition::HOST_SUFFIX, &pattern);
  EXPECT_EQ(URLMatcherCondition::HOST_SUFFIX, m1.criterion());
  EXPECT_EQ(&pattern, m1.string_pattern());

  URLMatcherCondition m2;
  m2 = m1;
  EXPECT_EQ(URLMatcherCondition::HOST_SUFFIX, m2.criterion());
  EXPECT_EQ(&pattern, m2.string_pattern());

  URLMatcherCondition m3(m1);
  EXPECT_EQ(URLMatcherCondition::HOST_SUFFIX, m3.criterion());
  EXPECT_EQ(&pattern, m3.string_pattern());
}

TEST(URLMatcherSchemeFilter, TestMatching) {
  URLMatcherSchemeFilter filter1("https");
  std::vector<std::string> filter2_content;
  filter2_content.push_back("http");
  filter2_content.push_back("https");
  URLMatcherSchemeFilter filter2(filter2_content);

  GURL matching_url("https://www.foobar.com");
  GURL non_matching_url("http://www.foobar.com");
  EXPECT_TRUE(filter1.IsMatch(matching_url));
  EXPECT_FALSE(filter1.IsMatch(non_matching_url));
  EXPECT_TRUE(filter2.IsMatch(matching_url));
  EXPECT_TRUE(filter2.IsMatch(non_matching_url));
}

TEST(URLMatcherPortFilter, TestMatching) {
  std::vector<URLMatcherPortFilter::Range> ranges;
  ranges.push_back(URLMatcherPortFilter::CreateRange(80, 90));
  ranges.push_back(URLMatcherPortFilter::CreateRange(8080));
  URLMatcherPortFilter filter(ranges);
  EXPECT_TRUE(filter.IsMatch(GURL("http://www.example.com")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://www.example.com:80")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://www.example.com:81")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://www.example.com:90")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://www.example.com:8080")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://www.example.com:79")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://www.example.com:91")));
  EXPECT_FALSE(filter.IsMatch(GURL("https://www.example.com")));
}

namespace {
void CreateAndAddCidrBlock(
    const std::string& cidr_block,
    std::vector<URLMatcherCidrBlockFilter::CidrBlock>& blocks) {
  auto block = URLMatcherCidrBlockFilter::CreateCidrBlock(cidr_block);
  ASSERT_TRUE(block.has_value());
  blocks.push_back(std::move(*block));
}
}  // namespace

TEST(URLMatcherCidrBlocksFilter, TestMatching_IPv4) {
  std::vector<URLMatcherCidrBlockFilter::CidrBlock> blocks;
  // 127.0.0.1/10 is equal to 127.0.0.1-127.63.255.255
  CreateAndAddCidrBlock("127.0.0.1/10", blocks);
  // 129.0.0.1/32 is equal to 129.0.0.1
  CreateAndAddCidrBlock("129.0.0.1/32", blocks);
  ASSERT_EQ(blocks.size(), 2u);

  URLMatcherCidrBlockFilter filter(std::move(blocks));
  EXPECT_TRUE(filter.IsMatch(GURL("http://129.0.0.1/test.html")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://129.0.0.1:80/test.html")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://127.0.0.1:81/test.html")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://127.63.255.255:90/test.html")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://127.63.0.255/test.html")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://129.0.0.2:79/test.html")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://127.64.255.255:91/test.html")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://127.64.255.255/test.html")));
  // Test that an IPv4 mapped IPv6 literal matches an IPv4 CIDR rule.
  EXPECT_TRUE(filter.IsMatch(GURL("http://[::ffff:127.63.0.255]/test.html")));
}

TEST(URLMatcherCidrBlocksFilter, TestMatching_IPv6) {
  std::vector<URLMatcherCidrBlockFilter::CidrBlock> blocks;
  CreateAndAddCidrBlock("a:b:c:d::/48", blocks);
  // This is the IPv4 mapped equivalent to 192.168.1.1/16.
  CreateAndAddCidrBlock("::ffff:192.168.1.1/112", blocks);
  ASSERT_EQ(blocks.size(), 2u);

  URLMatcherCidrBlockFilter filter(std::move(blocks));
  EXPECT_TRUE(filter.IsMatch(GURL("http://[A:b:C:9::]/test.html")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://foobar.com/test.html")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://192.169.1.1/test.html")));

  // Test that an IPv4 literal matches an IPv4 mapped IPv6 CIDR rule.
  EXPECT_TRUE(filter.IsMatch(GURL("http://[::ffff:192.168.1.3]/test.html")));
  EXPECT_TRUE(filter.IsMatch(GURL("http://192.168.11.11/test.html")));
  EXPECT_FALSE(filter.IsMatch(GURL("http://10.10.1.1/test.html")));

  // Test using an IP range that is close to IPv4 mapped, but not
  // quite. Should not result in matches.
  blocks.clear();
  CreateAndAddCidrBlock("::fffe:192.168.1.1/112", blocks);
  ASSERT_EQ(blocks.size(), 1u);

  URLMatcherCidrBlockFilter close_filter(std::move(blocks));
  EXPECT_TRUE(
      close_filter.IsMatch(GURL("http://[::fffe:192.168.1.3]/test.html")));
  EXPECT_FALSE(
      close_filter.IsMatch(GURL("http://[::ffff:192.168.1.3]/test.html")));
  EXPECT_FALSE(close_filter.IsMatch(GURL("http://192.168.11.11/test.html")));
  EXPECT_FALSE(close_filter.IsMatch(GURL("http://10.10.1.1/test.html")));
}

TEST(URLMatcherConditionTest, IsFullURLCondition) {
  MatcherStringPattern pattern("example.com", 1);
  EXPECT_FALSE(URLMatcherCondition(URLMatcherCondition::HOST_SUFFIX, &pattern)
                   .IsFullURLCondition());

  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::HOST_CONTAINS, &pattern)
                  .IsFullURLCondition());
  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::PATH_CONTAINS, &pattern)
                  .IsFullURLCondition());
  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::QUERY_CONTAINS, &pattern)
                  .IsFullURLCondition());

  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::URL_PREFIX, &pattern)
                  .IsFullURLCondition());
  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::URL_SUFFIX, &pattern)
                  .IsFullURLCondition());
  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::URL_CONTAINS, &pattern)
                  .IsFullURLCondition());
  EXPECT_TRUE(URLMatcherCondition(URLMatcherCondition::URL_EQUALS, &pattern)
                  .IsFullURLCondition());
}

TEST(URLMatcherConditionTest, IsMatch) {
  GURL url1("http://www.example.com/www.foobar.com/index.html");
  GURL url2("http://www.foobar.com/example.com/index.html");

  MatcherStringPattern pattern("example.com", 1);
  URLMatcherCondition m1(URLMatcherCondition::HOST_SUFFIX, &pattern);

  std::set<MatcherStringPattern::ID> matching_patterns;

  // matches = {0} --> matcher did not indicate that m1 was a match.
  matching_patterns.insert(0);
  EXPECT_FALSE(m1.IsMatch(matching_patterns, url1));

  // matches = {0, 1} --> matcher did indicate that m1 was a match.
  matching_patterns.insert(1);
  EXPECT_TRUE(m1.IsMatch(matching_patterns, url1));

  // For m2 we use a HOST_CONTAINS test, which requires a post-validation
  // whether the match reported by the SubstringSetMatcher occurs really
  // in the correct url component.
  URLMatcherCondition m2(URLMatcherCondition::HOST_CONTAINS, &pattern);
  EXPECT_TRUE(m2.IsMatch(matching_patterns, url1));
  EXPECT_FALSE(m2.IsMatch(matching_patterns, url2));
}

TEST(URLMatcherConditionTest, Comparison) {
  MatcherStringPattern p1("foobar.com", 1);
  MatcherStringPattern p2("foobar.com", 2);
  // The first component of each test is expected to be < than the second.
  URLMatcherCondition test_smaller[][2] = {
      {URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p1),
       URLMatcherCondition(URLMatcherCondition::HOST_SUFFIX, &p1)},
      {URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p1),
       URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p2)},
      {URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, nullptr),
       URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p2)},
      {URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p1),
       URLMatcherCondition(URLMatcherCondition::HOST_SUFFIX, nullptr)},
  };
  for (size_t i = 0; i < std::size(test_smaller); ++i) {
    EXPECT_TRUE(test_smaller[i][0] < test_smaller[i][1])
        << "Test " << i << " of test_smaller failed";
    EXPECT_FALSE(test_smaller[i][1] < test_smaller[i][0])
        << "Test " << i << " of test_smaller failed";
  }
  URLMatcherCondition test_equal[][2] = {
      {URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p1),
       URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, &p1)},
      {URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, nullptr),
       URLMatcherCondition(URLMatcherCondition::HOST_PREFIX, nullptr)},
  };
  for (size_t i = 0; i < std::size(test_equal); ++i) {
    EXPECT_FALSE(test_equal[i][0] < test_equal[i][1])
        << "Test " << i << " of test_equal failed";
    EXPECT_FALSE(test_equal[i][1] < test_equal[i][0])
        << "Test " << i << " of test_equal failed";
  }
}

//
// URLMatcherConditionFactory
//

namespace {

bool Matches(const URLMatcherCondition& condition, const std::string& text) {
  return text.find(condition.string_pattern()->pattern()) != std::string::npos;
}

}  // namespace

TEST(URLMatcherConditionFactoryTest, GURLCharacterSet) {
  // GURL guarantees that neither domain, nor path, nor query may contain
  // non ASCII-7 characters. We test this here, because a change to this
  // guarantee breaks this implementation horribly.
  GURL url("http://www.föö.com/föö?föö#föö");
  EXPECT_TRUE(base::IsStringASCII(url.host()));
  EXPECT_TRUE(base::IsStringASCII(url.path()));
  EXPECT_TRUE(base::IsStringASCII(url.query()));
  EXPECT_TRUE(base::IsStringASCII(url.ref()));
}

TEST(URLMatcherConditionFactoryTest, Criteria) {
  URLMatcherConditionFactory factory;
  EXPECT_EQ(URLMatcherCondition::HOST_PREFIX,
            factory.CreateHostPrefixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::HOST_SUFFIX,
            factory.CreateHostSuffixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::HOST_CONTAINS,
            factory.CreateHostContainsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::HOST_EQUALS,
            factory.CreateHostEqualsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::PATH_PREFIX,
            factory.CreatePathPrefixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::PATH_SUFFIX,
            factory.CreatePathSuffixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::PATH_CONTAINS,
            factory.CreatePathContainsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::PATH_EQUALS,
            factory.CreatePathEqualsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::QUERY_PREFIX,
            factory.CreateQueryPrefixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::QUERY_SUFFIX,
            factory.CreateQuerySuffixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::QUERY_CONTAINS,
            factory.CreateQueryContainsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::QUERY_EQUALS,
            factory.CreateQueryEqualsCondition("foo").criterion());
  EXPECT_EQ(
      URLMatcherCondition::HOST_SUFFIX_PATH_PREFIX,
      factory.CreateHostSuffixPathPrefixCondition("foo", "bar").criterion());
  EXPECT_EQ(
      URLMatcherCondition::HOST_EQUALS_PATH_PREFIX,
      factory.CreateHostEqualsPathPrefixCondition("foo", "bar").criterion());
  EXPECT_EQ(URLMatcherCondition::URL_PREFIX,
            factory.CreateURLPrefixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::URL_SUFFIX,
            factory.CreateURLSuffixCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::URL_CONTAINS,
            factory.CreateURLContainsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::URL_EQUALS,
            factory.CreateURLEqualsCondition("foo").criterion());
  EXPECT_EQ(URLMatcherCondition::URL_MATCHES,
            factory.CreateURLMatchesCondition("foo").criterion());
}

TEST(URLMatcherConditionFactoryTest, TestSingletonProperty) {
  URLMatcherConditionFactory factory;
  URLMatcherCondition c1 = factory.CreateHostEqualsCondition("www.google.com");
  URLMatcherCondition c2 = factory.CreateHostEqualsCondition("www.google.com");
  EXPECT_EQ(c1.criterion(), c2.criterion());
  EXPECT_EQ(c1.string_pattern(), c2.string_pattern());
  URLMatcherCondition c3 = factory.CreateHostEqualsCondition("www.google.de");
  EXPECT_EQ(c2.criterion(), c3.criterion());
  EXPECT_NE(c2.string_pattern(), c3.string_pattern());
  EXPECT_NE(c2.string_pattern()->id(), c3.string_pattern()->id());
  EXPECT_NE(c2.string_pattern()->pattern(), c3.string_pattern()->pattern());
  URLMatcherCondition c4 = factory.CreateURLMatchesCondition("www.google.com");
  URLMatcherCondition c5 = factory.CreateURLContainsCondition("www.google.com");
  // Regex patterns and substring patterns do not share IDs.
  EXPECT_EQ(c5.string_pattern()->pattern(), c4.string_pattern()->pattern());
  EXPECT_NE(c5.string_pattern(), c4.string_pattern());
  EXPECT_NE(c5.string_pattern()->id(), c4.string_pattern()->id());

  // Check that all MatcherStringPattern singletons are freed if we call
  // ForgetUnusedPatterns.
  MatcherStringPattern::ID old_id_1 = c1.string_pattern()->id();
  MatcherStringPattern::ID old_id_4 = c4.string_pattern()->id();
  factory.ForgetUnusedPatterns(std::set<MatcherStringPattern::ID>());
  EXPECT_TRUE(factory.IsEmpty());
  URLMatcherCondition c6 = factory.CreateHostEqualsCondition("www.google.com");
  EXPECT_NE(old_id_1, c6.string_pattern()->id());
  URLMatcherCondition c7 = factory.CreateURLMatchesCondition("www.google.com");
  EXPECT_NE(old_id_4, c7.string_pattern()->id());
}

TEST(URLMatcherConditionFactoryTest, TestComponentSearches) {
  URLMatcherConditionFactory factory;
  GURL gurl(
      "https://www.google.com:1234/webhp?sourceid=chrome-instant&ie=UTF-8"
      "&ion=1#hl=en&output=search&sclient=psy-ab&q=chrome%20is%20awesome");
  std::string url = factory.CanonicalizeURLForComponentSearches(gurl);
  GURL gurl2(
      "https://www.google.com.:1234/webhp?sourceid=chrome-instant"
      "&ie=UTF-8&ion=1#hl=en&output=search&sclient=psy-ab"
      "&q=chrome%20is%20awesome");
  std::string url2 = factory.CanonicalizeURLForComponentSearches(gurl2);

  // Test host component.
  EXPECT_TRUE(Matches(factory.CreateHostPrefixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreateHostPrefixCondition("www.goog"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostPrefixCondition("www.google.com"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostPrefixCondition(".www.google.com"), url));
  EXPECT_FALSE(Matches(factory.CreateHostPrefixCondition("google.com"), url));
  EXPECT_FALSE(
      Matches(factory.CreateHostPrefixCondition("www.google.com/"), url));
  EXPECT_FALSE(Matches(factory.CreateHostPrefixCondition("webhp"), url));

  EXPECT_TRUE(Matches(factory.CreateHostSuffixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreateHostSuffixCondition(std::string()), url2));
  EXPECT_TRUE(Matches(factory.CreateHostSuffixCondition("com"), url));
  EXPECT_TRUE(Matches(factory.CreateHostSuffixCondition("com"), url2));
  EXPECT_TRUE(Matches(factory.CreateHostSuffixCondition(".com"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostSuffixCondition("www.google.com"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostSuffixCondition(".www.google.com"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostSuffixCondition(".www.google.com"), url2));
  EXPECT_TRUE(
      Matches(factory.CreateHostSuffixCondition(".www.google.com."), url));
  EXPECT_FALSE(Matches(factory.CreateHostSuffixCondition("www"), url));
  EXPECT_FALSE(
      Matches(factory.CreateHostSuffixCondition("www.google.com/"), url));
  EXPECT_FALSE(Matches(factory.CreateHostSuffixCondition("webhp"), url));

  EXPECT_FALSE(Matches(factory.CreateHostEqualsCondition(std::string()), url));
  EXPECT_FALSE(Matches(factory.CreateHostEqualsCondition("www"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostEqualsCondition("www.google.com"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostEqualsCondition("www.google.com"), url2));
  EXPECT_FALSE(
      Matches(factory.CreateHostEqualsCondition("www.google.com/"), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostEqualsCondition(".www.google.com."), url));
  EXPECT_TRUE(
      Matches(factory.CreateHostEqualsCondition(".www.google.com."), url2));

  // Test path component.
  EXPECT_TRUE(Matches(factory.CreatePathPrefixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreatePathPrefixCondition("/web"), url));
  EXPECT_TRUE(Matches(factory.CreatePathPrefixCondition("/webhp"), url));
  EXPECT_FALSE(Matches(factory.CreatePathPrefixCondition("webhp"), url));
  EXPECT_FALSE(Matches(factory.CreatePathPrefixCondition("/webhp?"), url));
  EXPECT_FALSE(Matches(factory.CreatePathPrefixCondition("?sourceid"), url));

  EXPECT_TRUE(Matches(factory.CreatePathSuffixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreatePathSuffixCondition("webhp"), url));
  EXPECT_TRUE(Matches(factory.CreatePathSuffixCondition("/webhp"), url));
  EXPECT_FALSE(Matches(factory.CreatePathSuffixCondition("/web"), url));
  EXPECT_FALSE(Matches(factory.CreatePathSuffixCondition("/webhp?"), url));

  EXPECT_TRUE(Matches(factory.CreatePathEqualsCondition("/webhp"), url));
  EXPECT_FALSE(Matches(factory.CreatePathEqualsCondition("webhp"), url));
  EXPECT_FALSE(Matches(factory.CreatePathEqualsCondition("/webhp?"), url));
  EXPECT_FALSE(
      Matches(factory.CreatePathEqualsCondition("www.google.com"), url));

  // Test query component.
  EXPECT_TRUE(Matches(factory.CreateQueryPrefixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreateQueryPrefixCondition("sourceid"), url));
  // The '?' at the beginning is just ignored.
  EXPECT_TRUE(Matches(factory.CreateQueryPrefixCondition("?sourceid"), url));

  EXPECT_TRUE(Matches(factory.CreateQuerySuffixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreateQuerySuffixCondition("ion=1"), url));
  EXPECT_FALSE(Matches(factory.CreateQuerySuffixCondition("www"), url));
  // "Suffix" condition + pattern starting with '?' = "equals" condition.
  EXPECT_FALSE(Matches(factory.CreateQuerySuffixCondition(
                           "?sourceid=chrome-instant&ie=UTF-8&ion="),
                       url));
  EXPECT_TRUE(Matches(factory.CreateQuerySuffixCondition(
                          "?sourceid=chrome-instant&ie=UTF-8&ion=1"),
                      url));

  EXPECT_FALSE(Matches(factory.CreateQueryEqualsCondition(
                           "?sourceid=chrome-instant&ie=UTF-8&ion="),
                       url));
  EXPECT_FALSE(Matches(factory.CreateQueryEqualsCondition(
                           "sourceid=chrome-instant&ie=UTF-8&ion="),
                       url));
  EXPECT_TRUE(Matches(factory.CreateQueryEqualsCondition(
                          "sourceid=chrome-instant&ie=UTF-8&ion=1"),
                      url));
  // The '?' at the beginning is just ignored.
  EXPECT_TRUE(Matches(factory.CreateQueryEqualsCondition(
                          "?sourceid=chrome-instant&ie=UTF-8&ion=1"),
                      url));
  EXPECT_FALSE(
      Matches(factory.CreateQueryEqualsCondition("www.google.com"), url));

  // Test adjacent components
  EXPECT_TRUE(Matches(
      factory.CreateHostSuffixPathPrefixCondition("google.com", "/webhp"),
      url));
  EXPECT_TRUE(Matches(
      factory.CreateHostSuffixPathPrefixCondition("google.com", "/webhp"),
      url2));
  EXPECT_TRUE(Matches(
      factory.CreateHostSuffixPathPrefixCondition("google.com.", "/webhp"),
      url));
  EXPECT_TRUE(Matches(
      factory.CreateHostSuffixPathPrefixCondition("google.com.", "/webhp"),
      url2));
  EXPECT_TRUE(Matches(
      factory.CreateHostSuffixPathPrefixCondition(std::string(), "/webhp"),
      url));
  EXPECT_TRUE(Matches(
      factory.CreateHostSuffixPathPrefixCondition("google.com", std::string()),
      url));
  EXPECT_FALSE(Matches(
      factory.CreateHostSuffixPathPrefixCondition("www", std::string()), url));

  EXPECT_TRUE(Matches(
      factory.CreateHostEqualsPathPrefixCondition("www.google.com", "/webhp"),
      url));
  EXPECT_TRUE(Matches(
      factory.CreateHostEqualsPathPrefixCondition("www.google.com", "/webhp"),
      url2));
  EXPECT_TRUE(Matches(
      factory.CreateHostEqualsPathPrefixCondition(".www.google.com.", "/webhp"),
      url));
  EXPECT_TRUE(Matches(
      factory.CreateHostEqualsPathPrefixCondition(".www.google.com.", "/webhp"),
      url2));
  EXPECT_FALSE(Matches(
      factory.CreateHostEqualsPathPrefixCondition(std::string(), "/webhp"),
      url));
  EXPECT_TRUE(Matches(factory.CreateHostEqualsPathPrefixCondition(
                          "www.google.com", std::string()),
                      url));
  EXPECT_FALSE(Matches(
      factory.CreateHostEqualsPathPrefixCondition("google.com", std::string()),
      url));
}

TEST(URLMatcherConditionFactoryTest, TestFullSearches) {
  // The Port 443 is stripped because it is the default port for https.
  GURL gurl(
      "https://www.google.com:443/webhp?sourceid=chrome-instant&ie=UTF-8"
      "&ion=1#hl=en&output=search&sclient=psy-ab&q=chrome%20is%20awesome");
  URLMatcherConditionFactory factory;
  std::string url = factory.CanonicalizeURLForFullSearches(gurl);

  EXPECT_TRUE(Matches(factory.CreateURLPrefixCondition(std::string()), url));
  EXPECT_TRUE(
      Matches(factory.CreateURLPrefixCondition("https://www.goog"), url));
  EXPECT_TRUE(
      Matches(factory.CreateURLPrefixCondition("https://www.google.com"), url));
  EXPECT_TRUE(Matches(
      factory.CreateURLPrefixCondition("https://www.google.com/webhp?"), url));
  EXPECT_FALSE(
      Matches(factory.CreateURLPrefixCondition("http://www.google.com"), url));
  EXPECT_FALSE(Matches(factory.CreateURLPrefixCondition("webhp"), url));

  EXPECT_TRUE(Matches(factory.CreateURLSuffixCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreateURLSuffixCondition("ion=1"), url));
  EXPECT_FALSE(Matches(factory.CreateURLSuffixCondition("www"), url));

  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition(std::string()), url));
  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition("www.goog"), url));
  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition("webhp"), url));
  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition("?"), url));
  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition("sourceid"), url));
  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition("ion=1"), url));
  EXPECT_FALSE(Matches(factory.CreateURLContainsCondition(".www.goog"), url));
  EXPECT_FALSE(Matches(factory.CreateURLContainsCondition("foobar"), url));
  EXPECT_FALSE(Matches(factory.CreateURLContainsCondition("search"), url));
  EXPECT_FALSE(Matches(factory.CreateURLContainsCondition(":443"), url));

  EXPECT_TRUE(Matches(factory.CreateURLEqualsCondition(
                          "https://www.google.com/"
                          "webhp?sourceid=chrome-instant&ie=UTF-8&ion=1"),
                      url));
  EXPECT_FALSE(
      Matches(factory.CreateURLEqualsCondition("https://www.google.com"), url));

  // Same as above but this time with a non-standard port.
  gurl = GURL(
      "https://www.google.com:1234/webhp?sourceid=chrome-instant&"
      "ie=UTF-8&ion=1#hl=en&output=search&sclient=psy-ab&q=chrome%20is%20"
      "awesome");
  url = factory.CanonicalizeURLForFullSearches(gurl);
  EXPECT_TRUE(Matches(
      factory.CreateURLPrefixCondition("https://www.google.com:1234/webhp?"),
      url));
  EXPECT_TRUE(Matches(factory.CreateURLContainsCondition(":1234"), url));
}

//
// URLMatcherConditionSet
//

TEST(URLMatcherConditionSetTest, Constructor) {
  URLMatcherConditionFactory factory;
  URLMatcherCondition m1 = factory.CreateHostSuffixCondition("example.com");
  URLMatcherCondition m2 = factory.CreatePathContainsCondition("foo");

  std::set<URLMatcherCondition> conditions;
  conditions.insert(m1);
  conditions.insert(m2);

  scoped_refptr<URLMatcherConditionSet> condition_set(
      new URLMatcherConditionSet(1, conditions));
  EXPECT_EQ(1u, condition_set->id());
  EXPECT_EQ(2u, condition_set->conditions().size());
}

TEST(URLMatcherConditionSetTest, Matching) {
  GURL url1("http://www.example.com/foo?bar=1");
  GURL url2("http://foo.example.com/index.html");
  GURL url3("http://www.example.com:80/foo?bar=1");
  GURL url4("http://www.example.com:8080/foo?bar=1");

  URLMatcherConditionFactory factory;
  URLMatcherCondition m1 = factory.CreateHostSuffixCondition("example.com");
  URLMatcherCondition m2 = factory.CreatePathContainsCondition("foo");

  std::set<URLMatcherCondition> conditions;
  conditions.insert(m1);
  conditions.insert(m2);

  scoped_refptr<URLMatcherConditionSet> condition_set(
      new URLMatcherConditionSet(1, conditions));
  EXPECT_EQ(1u, condition_set->id());
  EXPECT_EQ(2u, condition_set->conditions().size());

  std::set<MatcherStringPattern::ID> matching_patterns;
  matching_patterns.insert(m1.string_pattern()->id());
  EXPECT_FALSE(condition_set->IsMatch(matching_patterns, url1));

  matching_patterns.insert(m2.string_pattern()->id());
  EXPECT_TRUE(condition_set->IsMatch(matching_patterns, url1));
  EXPECT_FALSE(condition_set->IsMatch(matching_patterns, url2));

  // Test scheme filters.
  scoped_refptr<URLMatcherConditionSet> condition_set2(
      new URLMatcherConditionSet(
          1, conditions, std::make_unique<URLMatcherSchemeFilter>("https"),
          std::unique_ptr<URLMatcherPortFilter>(),
          std::unique_ptr<URLMatcherCidrBlockFilter>()));
  EXPECT_FALSE(condition_set2->IsMatch(matching_patterns, url1));
  scoped_refptr<URLMatcherConditionSet> condition_set3(
      new URLMatcherConditionSet(
          1, conditions, std::make_unique<URLMatcherSchemeFilter>("http"),
          std::unique_ptr<URLMatcherPortFilter>(),
          std::unique_ptr<URLMatcherCidrBlockFilter>()));
  EXPECT_TRUE(condition_set3->IsMatch(matching_patterns, url1));

  // Test port filters.
  std::vector<URLMatcherPortFilter::Range> ranges;
  ranges.push_back(URLMatcherPortFilter::CreateRange(80));
  std::unique_ptr<URLMatcherPortFilter> filter(
      new URLMatcherPortFilter(ranges));
  scoped_refptr<URLMatcherConditionSet> condition_set4(
      new URLMatcherConditionSet(
          1, conditions, std::unique_ptr<URLMatcherSchemeFilter>(),
          std::move(filter), std::unique_ptr<URLMatcherCidrBlockFilter>()));
  EXPECT_TRUE(condition_set4->IsMatch(matching_patterns, url1));
  EXPECT_TRUE(condition_set4->IsMatch(matching_patterns, url3));
  EXPECT_FALSE(condition_set4->IsMatch(matching_patterns, url4));

  // Test regex patterns.
  matching_patterns.clear();
  URLMatcherCondition r1 = factory.CreateURLMatchesCondition("/fo?oo");
  std::set<URLMatcherCondition> regex_conditions;
  regex_conditions.insert(r1);
  scoped_refptr<URLMatcherConditionSet> condition_set5(
      new URLMatcherConditionSet(1, regex_conditions));
  EXPECT_FALSE(condition_set5->IsMatch(matching_patterns, url1));
  matching_patterns.insert(r1.string_pattern()->id());
  EXPECT_TRUE(condition_set5->IsMatch(matching_patterns, url1));

  regex_conditions.insert(m1);
  scoped_refptr<URLMatcherConditionSet> condition_set6(
      new URLMatcherConditionSet(1, regex_conditions));
  EXPECT_FALSE(condition_set6->IsMatch(matching_patterns, url1));
  matching_patterns.insert(m1.string_pattern()->id());
  EXPECT_TRUE(condition_set6->IsMatch(matching_patterns, url1));

  matching_patterns.clear();
  regex_conditions.clear();
  URLMatcherCondition r2 = factory.CreateOriginAndPathMatchesCondition("b[a]r");
  regex_conditions.insert(r2);
  scoped_refptr<URLMatcherConditionSet> condition_set7(
      new URLMatcherConditionSet(1, regex_conditions));
  EXPECT_FALSE(condition_set7->IsMatch(matching_patterns, url1));
  matching_patterns.insert(r2.string_pattern()->id());
  EXPECT_TRUE(condition_set7->IsMatch(matching_patterns, url1));
}

namespace {

bool IsQueryMatch(
    const std::string& url_query,
    const std::string& key,
    URLQueryElementMatcherCondition::QueryElementType query_element_type,
    const std::string& value,
    URLQueryElementMatcherCondition::QueryValueMatchType query_value_match_type,
    URLQueryElementMatcherCondition::Type match_type) {
  URLMatcherConditionFactory factory;

  URLMatcherCondition m1 = factory.CreateHostSuffixCondition("example.com");
  URLMatcherCondition m2 = factory.CreatePathContainsCondition("foo");
  URLMatcherConditionSet::Conditions conditions;
  conditions.insert(m1);
  conditions.insert(m2);

  URLQueryElementMatcherCondition q1(key, value, query_value_match_type,
                                     query_element_type, match_type, &factory);
  URLMatcherConditionSet::QueryConditions query_conditions;
  query_conditions.insert(q1);

  std::unique_ptr<URLMatcherSchemeFilter> scheme_filter;
  std::unique_ptr<URLMatcherPortFilter> port_filter;

  scoped_refptr<URLMatcherConditionSet> condition_set(
      new URLMatcherConditionSet(1, conditions, query_conditions,
                                 std::move(scheme_filter),
                                 std::move(port_filter)));

  GURL url("http://www.example.com/foo?" + url_query);

  URLMatcher matcher;
  URLMatcherConditionSet::Vector vector;
  vector.push_back(condition_set);
  matcher.AddConditionSets(vector);

  return matcher.MatchURL(url).size() == 1;
}

}  // namespace

TEST(URLMatcherConditionSetTest, QueryMatching) {
  EXPECT_TRUE(IsQueryMatch(
      "a=foo&b=foo&a=barr", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ANY));
  EXPECT_FALSE(IsQueryMatch(
      "a=foo&b=foo&a=barr", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ANY));
  EXPECT_TRUE(
      IsQueryMatch("a=foo&b=foo&a=barr", "a",
                   URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY, "bar",
                   URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
                   URLQueryElementMatcherCondition::MATCH_ANY));
  EXPECT_FALSE(
      IsQueryMatch("a=foo&b=foo&a=barr", "a",
                   URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY, "bar",
                   URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
                   URLQueryElementMatcherCondition::MATCH_ANY));
  EXPECT_TRUE(IsQueryMatch(
      "a&b=foo&a=barr", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "bar", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ANY));
  EXPECT_FALSE(
      IsQueryMatch("a=foo&b=foo&a=barr", "a",
                   URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY, "bar",
                   URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
                   URLQueryElementMatcherCondition::MATCH_ANY));

  EXPECT_FALSE(IsQueryMatch(
      "a=foo&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "a=bar&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "a=bar&b=foo&a=bar", "b",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_FALSE(IsQueryMatch(
      "a=bar&b=foo&a=bar", "b",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "goo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_FALSE(IsQueryMatch(
      "a=bar&b=foo&a=bar", "c",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "goo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "a=foo1&b=foo&a=foo2", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_FALSE(IsQueryMatch(
      "a=foo1&b=foo&a=fo02", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "a&b=foo&a", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "alt&b=foo", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "b=foo&a", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_FALSE(IsQueryMatch(
      "b=foo", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_ALL));
  EXPECT_TRUE(IsQueryMatch(
      "b=foo&a", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_ALL));

  EXPECT_TRUE(IsQueryMatch(
      "a=foo&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_FIRST));
  EXPECT_FALSE(IsQueryMatch(
      "a=foo&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_FIRST));
  EXPECT_TRUE(IsQueryMatch(
      "a=foo1&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_FIRST));
  EXPECT_FALSE(IsQueryMatch(
      "a=foo1&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_FIRST));
  EXPECT_TRUE(IsQueryMatch(
      "a&b=foo&a=bar", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_FIRST));
  EXPECT_TRUE(IsQueryMatch(
      "alt&b=foo&a=bar", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_FIRST));
  EXPECT_FALSE(IsQueryMatch(
      "alt&b=foo&a=bar", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_FIRST));

  EXPECT_FALSE(IsQueryMatch(
      "a=foo&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_LAST));
  EXPECT_TRUE(IsQueryMatch(
      "a=foo&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_LAST));
  EXPECT_FALSE(IsQueryMatch(
      "a=foo1&b=foo&a=bar", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "foo",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_LAST));
  EXPECT_TRUE(IsQueryMatch(
      "a=foo1&b=foo&a=bar1", "a",
      URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE, "bar",
      URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_LAST));
  EXPECT_FALSE(IsQueryMatch(
      "a&b=foo&a=bar", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_LAST));
  EXPECT_TRUE(IsQueryMatch(
      "b=foo&alt", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX,
      URLQueryElementMatcherCondition::MATCH_LAST));
  EXPECT_FALSE(IsQueryMatch(
      "b=foo&alt", "a", URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY,
      "foo", URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT,
      URLQueryElementMatcherCondition::MATCH_LAST));
}

//
// URLMatcher
//

TEST(URLMatcherTest, FullTest) {
  GURL url1("http://www.example.com/foo?bar=1");
  GURL url2("http://foo.example.com/index.html");

  URLMatcher matcher;
  URLMatcherConditionFactory* factory = matcher.condition_factory();

  // First insert.
  URLMatcherConditionSet::Conditions conditions1;
  conditions1.insert(factory->CreateHostSuffixCondition("example.com"));
  conditions1.insert(factory->CreatePathContainsCondition("foo"));

  const base::MatcherStringPattern::ID kConditionSetId1 = 1;
  URLMatcherConditionSet::Vector insert1;
  insert1.push_back(base::MakeRefCounted<URLMatcherConditionSet>(
      kConditionSetId1, conditions1));
  matcher.AddConditionSets(insert1);
  EXPECT_EQ(1u, matcher.MatchURL(url1).size());
  EXPECT_EQ(0u, matcher.MatchURL(url2).size());

  // Second insert.
  URLMatcherConditionSet::Conditions conditions2;
  conditions2.insert(factory->CreateHostSuffixCondition("example.com"));

  const base::MatcherStringPattern::ID kConditionSetId2 = 2;
  URLMatcherConditionSet::Vector insert2;
  insert2.push_back(base::MakeRefCounted<URLMatcherConditionSet>(
      kConditionSetId2, conditions2));
  matcher.AddConditionSets(insert2);
  EXPECT_EQ(2u, matcher.MatchURL(url1).size());
  EXPECT_EQ(1u, matcher.MatchURL(url2).size());

  // This should be the cached singleton.
  base::MatcherStringPattern::ID patternId1 =
      factory->CreateHostSuffixCondition("example.com").string_pattern()->id();

  // Third insert.
  URLMatcherConditionSet::Conditions conditions3;
  conditions3.insert(factory->CreateHostSuffixCondition("example.com"));
  conditions3.insert(factory->CreateURLMatchesCondition("x.*[0-9]"));

  const base::MatcherStringPattern::ID kConditionSetId3 = 3;
  URLMatcherConditionSet::Vector insert3;
  insert3.push_back(base::MakeRefCounted<URLMatcherConditionSet>(
      kConditionSetId3, conditions3));
  matcher.AddConditionSets(insert3);
  EXPECT_EQ(3u, matcher.MatchURL(url1).size());
  EXPECT_EQ(1u, matcher.MatchURL(url2).size());

  // Removal of third insert.
  std::vector<base::MatcherStringPattern::ID> remove3;
  remove3.push_back(kConditionSetId3);
  matcher.RemoveConditionSets(remove3);
  EXPECT_EQ(2u, matcher.MatchURL(url1).size());
  EXPECT_EQ(1u, matcher.MatchURL(url2).size());

  // Removal of second insert.
  std::vector<base::MatcherStringPattern::ID> remove2;
  remove2.push_back(kConditionSetId2);
  matcher.RemoveConditionSets(remove2);
  EXPECT_EQ(1u, matcher.MatchURL(url1).size());
  EXPECT_EQ(0u, matcher.MatchURL(url2).size());

  // Removal of first insert.
  std::vector<base::MatcherStringPattern::ID> remove1;
  remove1.push_back(kConditionSetId1);
  matcher.RemoveConditionSets(remove1);
  EXPECT_EQ(0u, matcher.MatchURL(url1).size());
  EXPECT_EQ(0u, matcher.MatchURL(url2).size());

  EXPECT_TRUE(matcher.IsEmpty());

  // The cached singleton in matcher.condition_factory_ should be destroyed to
  // free memory.
  base::MatcherStringPattern::ID patternId2 =
      factory->CreateHostSuffixCondition("example.com").string_pattern()->id();
  // If patternId1 and patternId2 are different that indicates that
  // matcher.condition_factory_ does not leak memory by holding onto
  // unused patterns.
  EXPECT_NE(patternId1, patternId2);
}

TEST(URLMatcherTest, TestComponentsImplyContains) {
  // Due to a different implementation of component (prefix, suffix and equals)
  // and *Contains conditions we need to check that when a pattern matches a
  // given part of a URL as equal, prefix or suffix, it also matches it in the
  // "contains" test.
  GURL url("https://www.google.com:1234/webhp?test=val&a=b");

  URLMatcher matcher;
  URLMatcherConditionFactory* factory = matcher.condition_factory();

  URLMatcherConditionSet::Conditions conditions;

  // First insert all the matching equals => contains pairs.
  conditions.insert(factory->CreateHostEqualsCondition("www.google.com"));
  conditions.insert(factory->CreateHostContainsCondition("www.google.com"));

  conditions.insert(factory->CreateHostPrefixCondition("www."));
  conditions.insert(factory->CreateHostContainsCondition("www."));

  conditions.insert(factory->CreateHostSuffixCondition("com"));
  conditions.insert(factory->CreateHostContainsCondition("com"));

  conditions.insert(factory->CreatePathEqualsCondition("/webhp"));
  conditions.insert(factory->CreatePathContainsCondition("/webhp"));

  conditions.insert(factory->CreatePathPrefixCondition("/we"));
  conditions.insert(factory->CreatePathContainsCondition("/we"));

  conditions.insert(factory->CreatePathSuffixCondition("hp"));
  conditions.insert(factory->CreatePathContainsCondition("hp"));

  conditions.insert(factory->CreateQueryEqualsCondition("test=val&a=b"));
  conditions.insert(factory->CreateQueryContainsCondition("test=val&a=b"));

  conditions.insert(factory->CreateQueryPrefixCondition("test=v"));
  conditions.insert(factory->CreateQueryContainsCondition("test=v"));

  conditions.insert(factory->CreateQuerySuffixCondition("l&a=b"));
  conditions.insert(factory->CreateQueryContainsCondition("l&a=b"));

  // The '?' for equality is just ignored.
  conditions.insert(factory->CreateQueryEqualsCondition("?test=val&a=b"));
  // Due to '?' the condition created here is a prefix-testing condition.
  conditions.insert(factory->CreateQueryContainsCondition("?test=val&a=b"));

  const base::MatcherStringPattern::ID kConditionSetId = 1;
  URLMatcherConditionSet::Vector insert;
  insert.push_back(base::MakeRefCounted<URLMatcherConditionSet>(kConditionSetId,
                                                                conditions));
  matcher.AddConditionSets(insert);
  EXPECT_EQ(1u, matcher.MatchURL(url).size());
}

// Check that matches in everything but the query are found.
TEST(URLMatcherTest, TestOriginAndPathRegExPositive) {
  GURL url("https://www.google.com:1234/webhp?test=val&a=b");

  URLMatcher matcher;
  URLMatcherConditionFactory* factory = matcher.condition_factory();

  URLMatcherConditionSet::Conditions conditions;

  conditions.insert(factory->CreateOriginAndPathMatchesCondition("w..hp"));
  const base::MatcherStringPattern::ID kConditionSetId = 1;
  URLMatcherConditionSet::Vector insert;
  insert.push_back(base::MakeRefCounted<URLMatcherConditionSet>(kConditionSetId,
                                                                conditions));
  matcher.AddConditionSets(insert);
  EXPECT_EQ(1u, matcher.MatchURL(url).size());
}

// Check that matches in the query are ignored.
TEST(URLMatcherTest, TestOriginAndPathRegExNegative) {
  GURL url("https://www.google.com:1234/webhp?test=val&a=b");

  URLMatcher matcher;
  URLMatcherConditionFactory* factory = matcher.condition_factory();

  URLMatcherConditionSet::Conditions conditions;

  conditions.insert(factory->CreateOriginAndPathMatchesCondition("val"));
  const base::MatcherStringPattern::ID kConditionSetId = 1;
  URLMatcherConditionSet::Vector insert;
  insert.push_back(base::MakeRefCounted<URLMatcherConditionSet>(kConditionSetId,
                                                                conditions));
  matcher.AddConditionSets(insert);
  EXPECT_EQ(0u, matcher.MatchURL(url).size());
}

}  // namespace url_matcher
