// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/v4_protocol_manager_util.h"

#include <vector>

#include "base/base64.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "net/base/escape.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace safe_browsing {

class V4ProtocolManagerUtilTest : public testing::Test {};

TEST_F(V4ProtocolManagerUtilTest, TestBackOffLogic) {
  size_t error_count = 0, back_off_multiplier = 1;

  // 1 error.
  base::TimeDelta next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &error_count, &back_off_multiplier);
  EXPECT_EQ(1U, error_count);
  EXPECT_EQ(1U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(15), next);
  EXPECT_GE(TimeDelta::FromMinutes(30), next);

  // 2 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(2U, error_count);
  EXPECT_EQ(2U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(30), next);
  EXPECT_GE(TimeDelta::FromMinutes(60), next);

  // 3 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(3U, error_count);
  EXPECT_EQ(4U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(60), next);
  EXPECT_GE(TimeDelta::FromMinutes(120), next);

  // 4 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(4U, error_count);
  EXPECT_EQ(8U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(120), next);
  EXPECT_GE(TimeDelta::FromMinutes(240), next);

  // 5 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(5U, error_count);
  EXPECT_EQ(16U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(240), next);
  EXPECT_GE(TimeDelta::FromMinutes(480), next);

  // 6 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(6U, error_count);
  EXPECT_EQ(32U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(480), next);
  EXPECT_GE(TimeDelta::FromMinutes(960), next);

  // 7 errors.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(7U, error_count);
  EXPECT_EQ(64U, back_off_multiplier);
  EXPECT_LE(TimeDelta::FromMinutes(960), next);
  EXPECT_GE(TimeDelta::FromMinutes(1920), next);

  // 8 errors, reached max backoff.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(8U, error_count);
  EXPECT_EQ(128U, back_off_multiplier);
  EXPECT_EQ(TimeDelta::FromHours(24), next);

  // 9 errors, reached max backoff and multiplier capped.
  next = V4ProtocolManagerUtil::GetNextBackOffInterval(&error_count,
                                                       &back_off_multiplier);
  EXPECT_EQ(9U, error_count);
  EXPECT_EQ(128U, back_off_multiplier);
  EXPECT_EQ(TimeDelta::FromHours(24), next);
}

TEST_F(V4ProtocolManagerUtilTest, TestGetRequestUrlAndUpdateHeaders) {
  net::HttpRequestHeaders headers;
  GURL gurl;
  V4ProtocolManagerUtil::GetRequestUrlAndHeaders("request_base64", "someMethod",
                                                 GetTestV4ProtocolConfig(),
                                                 &gurl, &headers);
  std::string expectedUrl =
      "https://safebrowsing.googleapis.com/v4/someMethod?"
      "$req=request_base64&$ct=application/x-protobuf&key=test_key_param";
  EXPECT_EQ(expectedUrl, gurl.spec());
  std::string header_value;
  EXPECT_TRUE(headers.GetHeader("X-HTTP-Method-Override", &header_value));
  EXPECT_EQ("POST", header_value);
}

// Tests that we generate the required host/path combinations for testing
// according to the Safe Browsing spec.
// See: https://developers.google.com/safe-browsing/v4/urls-hashing
TEST_F(V4ProtocolManagerUtilTest, UrlParsing) {
  std::vector<std::string> hosts, paths;

  GURL url("http://a.b.c/1/2.html?param=1");
  V4ProtocolManagerUtil::GenerateHostsToCheck(url, &hosts);
  V4ProtocolManagerUtil::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(2));
  EXPECT_EQ(paths.size(), static_cast<size_t>(4));
  EXPECT_EQ(hosts[0], "b.c");
  EXPECT_EQ(hosts[1], "a.b.c");

  EXPECT_TRUE(base::Contains(paths, "/1/2.html?param=1"));
  EXPECT_TRUE(base::Contains(paths, "/1/2.html"));
  EXPECT_TRUE(base::Contains(paths, "/1/"));
  EXPECT_TRUE(base::Contains(paths, "/"));

  url = GURL("http://a.b.c.d.e.f.g/1.html");
  V4ProtocolManagerUtil::GenerateHostsToCheck(url, &hosts);
  V4ProtocolManagerUtil::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(5));
  EXPECT_EQ(paths.size(), static_cast<size_t>(2));
  EXPECT_EQ(hosts[0], "f.g");
  EXPECT_EQ(hosts[1], "e.f.g");
  EXPECT_EQ(hosts[2], "d.e.f.g");
  EXPECT_EQ(hosts[3], "c.d.e.f.g");
  EXPECT_EQ(hosts[4], "a.b.c.d.e.f.g");
  EXPECT_TRUE(base::Contains(paths, "/1.html"));
  EXPECT_TRUE(base::Contains(paths, "/"));

  url = GURL("http://a.b/saw-cgi/eBayISAPI.dll/");
  V4ProtocolManagerUtil::GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(paths.size(), static_cast<size_t>(3));
  EXPECT_TRUE(base::Contains(paths, "/saw-cgi/eBayISAPI.dll/"));
  EXPECT_TRUE(base::Contains(paths, "/saw-cgi/"));
  EXPECT_TRUE(base::Contains(paths, "/"));
}

// Tests the url canonicalization according to the Safe Browsing spec.
// See: https://developers.google.com/safe-browsing/v4/urls-hashing
TEST_F(V4ProtocolManagerUtilTest, CanonicalizeUrl) {
  struct {
    const char* input_url;
    const char* expected_canonicalized_hostname;
    const char* expected_canonicalized_path;
    const char* expected_canonicalized_query;
  } tests[] = {
      {"http://host/%25%32%35", "host", "/%25", ""},
      {"http://host/%25%32%35%25%32%35", "host", "/%25%25", ""},
      {"http://host/%2525252525252525", "host", "/%25", ""},
      {"http://host/asdf%25%32%35asd", "host", "/asdf%25asd", ""},
      {"http://host/%%%25%32%35asd%%", "host", "/%25%25%25asd%25%25", ""},
      {"http://host/%%%25%32%35asd%%", "host", "/%25%25%25asd%25%25", ""},
      {"http://www.google.com/", "www.google.com", "/", ""},
      {"http://%31%36%38%2e%31%38%38%2e%39%39%2e%32%36/%2E%73%65%63%75%72%65/"
       "%77"
       "%77%77%2E%65%62%61%79%2E%63%6F%6D/",
       "168.188.99.26", "/.secure/www.ebay.com/", ""},
      {"http://195.127.0.11/uploads/%20%20%20%20/.verify/"
       ".eBaysecure=updateuserd"
       "ataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/",
       "195.127.0.11",
       "/uploads/%20%20%20%20/.verify/"
       ".eBaysecure=updateuserdataxplimnbqmn-xplmv"
       "alidateinfoswqpcmlx=hgplmcx/",
       ""},
      {"http://host.com/%257Ea%2521b%2540c%2523d%2524e%25f%255E00%252611%252A"
       "22%252833%252944_55%252B",
       "host.com", "/~a!b@c%23d$e%25f^00&11*22(33)44_55+", ""},
      {"http://3279880203/blah", "195.127.0.11", "/blah", ""},
      {"http://www.google.com/blah/..", "www.google.com", "/", ""},
      {"http://www.google.com/blah#fraq", "www.google.com", "/blah", ""},
      {"http://www.GOOgle.com/", "www.google.com", "/", ""},
      {"http://www.google.com.../", "www.google.com", "/", ""},
      {"http://www.google.com/q?", "www.google.com", "/q", ""},
      {"http://www.google.com/q?r?", "www.google.com", "/q", "r?"},
      {"http://www.google.com/q?r?s", "www.google.com", "/q", "r?s"},
      {"http://evil.com/foo#bar#baz", "evil.com", "/foo", ""},
      {"http://evil.com/foo;", "evil.com", "/foo;", ""},
      {"http://evil.com/foo?bar;", "evil.com", "/foo", "bar;"},
      {"http://notrailingslash.com", "notrailingslash.com", "/", ""},
      {"http://www.gotaport.com:1234/", "www.gotaport.com", "/", ""},
      {"  http://www.google.com/  ", "www.google.com", "/", ""},
      {"http:// leadingspace.com/", "%20leadingspace.com", "/", ""},
      {"http://%20leadingspace.com/", "%20leadingspace.com", "/", ""},
      {"https://www.securesite.com/", "www.securesite.com", "/", ""},
      {"http://host.com/ab%23cd", "host.com", "/ab%23cd", ""},
      {"http://host%3e.com//twoslashes?more//slashes", "host>.com",
       "/twoslashes", "more//slashes"},
      {"http://host.com/abc?val=xyz#anything", "host.com", "/abc", "val=xyz"},
      {"http://abc:def@host.com/xyz", "host.com", "/xyz", ""},
      {"http://host%3e.com/abc/%2e%2e%2fdef", "host>.com", "/def", ""},
      {"http://.......host...com.....//abc/////def%2F%2F%2Fxyz", "host.com",
       "/abc/def/xyz", ""},
      {"ftp://host.com/foo?bar", "host.com", "/foo", "bar"},
      {"data:text/html;charset=utf-8,%0D%0A", "", "", ""},
      {"javascript:alert()", "", "", ""},
      {"mailto:abc@example.com", "", "", ""},
  };
  for (size_t i = 0; i < base::size(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test: %s", tests[i].input_url));
    GURL url(tests[i].input_url);

    std::string canonicalized_hostname;
    std::string canonicalized_path;
    std::string canonicalized_query;
    V4ProtocolManagerUtil::CanonicalizeUrl(url, &canonicalized_hostname,
                                           &canonicalized_path,
                                           &canonicalized_query);

    EXPECT_EQ(tests[i].expected_canonicalized_hostname, canonicalized_hostname);
    EXPECT_EQ(tests[i].expected_canonicalized_path, canonicalized_path);
    EXPECT_EQ(tests[i].expected_canonicalized_query, canonicalized_query);
  }
}

TEST_F(V4ProtocolManagerUtilTest, TestIPAddressToEncodedIPV6) {
  // To verify the test values, here's the python code:
  // >> import socket, hashlib, binascii
  // >> hashlib.sha1(socket.inet_pton(socket.AF_INET6, input)).digest() +
  // chr(128)
  // For example:
  // >>> hashlib.sha1(socket.inet_pton(socket.AF_INET6,
  // '::ffff:192.168.1.1')).digest() + chr(128)
  // 'X\xf8\xa1\x17I\xe6Pl\xfd\xdb\xbb\xa0\x0c\x02\x9d#\n|\xe7\xcd\x80'
  std::vector<std::tuple<bool, std::string, std::string>> test_cases = {
      std::make_tuple(false, "", ""),
      std::make_tuple(
          true, "192.168.1.1",
          "X\xF8\xA1\x17I\xE6Pl\xFD\xDB\xBB\xA0\f\x2\x9D#\n|\xE7\xCD\x80"),
      std::make_tuple(
          true,
          "::", "\xE1)\xF2|Q\x3\xBC\\\xC4K\xCD\xF0\xA1^\x16\rDPf\xFF\x80")};
  for (size_t i = 0; i < test_cases.size(); i++) {
    DVLOG(1) << "Running case: " << i;
    bool success = std::get<0>(test_cases[i]);
    const auto& input = std::get<1>(test_cases[i]);
    const auto& expected_output = std::get<2>(test_cases[i]);
    std::string encoded_ip;
    ASSERT_EQ(success, V4ProtocolManagerUtil::IPAddressToEncodedIPV6Hash(
                           input, &encoded_ip));
    if (success) {
      ASSERT_EQ(expected_output, encoded_ip);
    }
  }
}

TEST_F(V4ProtocolManagerUtilTest, TestFullHashToHashPrefix) {
  const std::string full_hash = "abcdefgh";
  std::vector<std::tuple<bool, std::string, PrefixSize, std::string>>
      test_cases = {
          std::make_tuple(true, "", 0, ""),
          std::make_tuple(false, "", kMinHashPrefixLength, ""),
          std::make_tuple(true, "a", 1, full_hash),
          std::make_tuple(true, "abcd", kMinHashPrefixLength, full_hash),
          std::make_tuple(true, "abcde", kMinHashPrefixLength + 1, full_hash)};
  for (size_t i = 0; i < test_cases.size(); i++) {
    DVLOG(1) << "Running case: " << i;
    bool success = std::get<0>(test_cases[i]);
    const auto& expected_prefix = std::get<1>(test_cases[i]);
    const PrefixSize& prefix_size = std::get<2>(test_cases[i]);
    const auto& input_full_hash = std::get<3>(test_cases[i]);
    std::string prefix;
    ASSERT_EQ(success, V4ProtocolManagerUtil::FullHashToHashPrefix(
                           input_full_hash, prefix_size, &prefix));
    if (success) {
      ASSERT_EQ(expected_prefix, prefix);
    }
  }
}

}  // namespace safe_browsing
