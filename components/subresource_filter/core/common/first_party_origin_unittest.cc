// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/first_party_origin.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(FirstPartyOriginTest, AllSameDomain) {
  const std::string kDomain = "sub.example.co.uk";

  FirstPartyOrigin first_party(url::Origin::Create(GURL("https://" + kDomain)));
  for (int index = 0; index < 5; ++index) {
    GURL url("https://" + kDomain + "/path?q=" + base::NumberToString(index));
    EXPECT_FALSE(FirstPartyOrigin::IsThirdParty(url, first_party.origin()));
    EXPECT_FALSE(first_party.IsThirdParty(url));
  }
}

TEST(FirstPartyOriginTest, AllFirstParty) {
  const std::string kDomain = "example.co.uk";

  FirstPartyOrigin first_party(url::Origin::Create(GURL("https://" + kDomain)));
  for (int index = 0; index < 5; ++index) {
    GURL url("https://sub" + base::NumberToString(index) + "." + kDomain +
             "/suf");
    EXPECT_FALSE(FirstPartyOrigin::IsThirdParty(url, first_party.origin()));
    EXPECT_FALSE(first_party.IsThirdParty(url));
  }
}

TEST(FirstPartyOriginTest, AllThirdParty) {
  const std::string kDomain = "example.co.uk";

  FirstPartyOrigin first_party(url::Origin::Create(GURL("https://" + kDomain)));
  for (int index = 0; index < 5; ++index) {
    GURL url("https://example" + base::NumberToString(index) +
             ".co.uk/path?k=v");
    EXPECT_TRUE(FirstPartyOrigin::IsThirdParty(url, first_party.origin()));
    EXPECT_TRUE(first_party.IsThirdParty(url));
  }
}

TEST(FirstPartyOriginTest, MixedFirstAndThirdParties) {
  const struct {
    const char* url;
    bool is_third_party;
  } kTestCases[] = {
      {"https://sub.example.com", false},
      {"https://subexample.com", true},
      {"https://sub.subexample.com", true},
      {"https://sub.sub.example.com", false},
      {"https://xample.com", true},
      {"https://sub.xample.com", true},
      {"data:text/plain,example.com", true},
  };

  FirstPartyOrigin first_party(
      url::Origin::Create(GURL("https://example.com")));
  for (const auto& test_case : kTestCases) {
    GURL url(test_case.url);
    EXPECT_EQ(test_case.is_third_party,
              FirstPartyOrigin::IsThirdParty(url, first_party.origin()));
    EXPECT_EQ(test_case.is_third_party, first_party.IsThirdParty(url));
  }
}

TEST(FirstPartyOriginTest, EmptyHostUrls) {
  const char* const kUrls[] = {
      "data:text/plain,example.com", "data:text/plain,another.example.com",
      "data:text/plain;base64,ABACABA",
  };

  FirstPartyOrigin first_party(
      url::Origin::Create(GURL("https://example.com")));
  for (auto* url_string : kUrls) {
    GURL url(url_string);
    EXPECT_TRUE(FirstPartyOrigin::IsThirdParty(url, first_party.origin()));
    EXPECT_TRUE(first_party.IsThirdParty(url));
  }
}

}  // namespace subresource_filter
