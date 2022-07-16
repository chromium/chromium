// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/url_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace url_utils {
namespace {

using testing::Eq;

TEST(UrlUtilsTest, IsInDomainOrSubDomain) {
  std::vector<std::string> allowed_domains = {"example.com",
                                              "other-example.com"};
  EXPECT_TRUE(IsInDomainOrSubDomain(GURL("http://a.example.com/"),
                                    GURL("http://example.com")));
  EXPECT_TRUE(
      IsInDomainOrSubDomain(GURL("http://a.example.com/"), allowed_domains));

  EXPECT_FALSE(IsInDomainOrSubDomain(GURL("http://other-example.com/"),
                                     GURL("http://example.com")));
  EXPECT_TRUE(IsInDomainOrSubDomain(GURL("http://other-example.com/"),
                                    allowed_domains));

  EXPECT_FALSE(IsInDomainOrSubDomain(GURL("http://sub.other-example.com/"),
                                     GURL("http://example.com")));
  EXPECT_TRUE(IsInDomainOrSubDomain(GURL("http://sub.other-example.com/"),
                                    allowed_domains));

  EXPECT_FALSE(IsInDomainOrSubDomain(GURL("http://example.different.com/"),
                                     GURL("http://example.com")));
  EXPECT_FALSE(IsInDomainOrSubDomain(GURL("http://example.different.com/"),
                                     allowed_domains));
}

TEST(UrlUtilsTest, IsSamePublicSuffixDomain) {
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("http://www.example.com"),
                                       GURL("http://sub.example.com")));
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("http://example.com"),
                                       GURL("http://sub.example.com")));
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("http://www.example.com"),
                                       GURL("https://www.example.com")));
  EXPECT_FALSE(IsSamePublicSuffixDomain(GURL("http://www.example.com"),
                                        GURL("http://www.other.com")));
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("http://127.0.0.1/a"),
                                       GURL("http://127.0.0.1/b")));
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("http://www.example.com/a"),
                                       GURL("http://www.example.com/b")));
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("https://www.example.com:443"),
                                       GURL("https://sub.example.com:8080")));
  EXPECT_TRUE(IsSamePublicSuffixDomain(GURL("http://www.example.co.uk"),
                                       GURL("http://sub.example.co.uk")));
  EXPECT_FALSE(IsSamePublicSuffixDomain(GURL("http://example.com"),
                                        GURL("http://example.ch")));
  EXPECT_FALSE(
      IsSamePublicSuffixDomain(GURL("http://example.com"), GURL("invalid")));
  EXPECT_FALSE(IsSamePublicSuffixDomain(GURL("invalid"), GURL("invalid")));
}

TEST(UrlUtilsTest, GetOrganizationIdentifyingDomain) {
  EXPECT_THAT(GetOrganizationIdentifyingDomain(GURL("https://www.example.com")),
              Eq("example.com"));
  EXPECT_THAT(
      GetOrganizationIdentifyingDomain(GURL("https://subdomain.example.com")),
      Eq("example.com"));
  EXPECT_THAT(GetOrganizationIdentifyingDomain(GURL("https://example.com")),
              Eq("example.com"));
}

TEST(UrlUtilsTest, IsAllowedSchemaTransition) {
  EXPECT_TRUE(IsAllowedSchemaTransition(GURL("http://example.com"),
                                        GURL("http://example.com")));
  EXPECT_TRUE(IsAllowedSchemaTransition(GURL("https://example.com"),
                                        GURL("https://example.com")));
  EXPECT_TRUE(IsAllowedSchemaTransition(GURL("http://example.com"),
                                        GURL("https://example.com")));
  EXPECT_FALSE(IsAllowedSchemaTransition(GURL("https://example.com"),
                                         GURL("http://example.com")));
}

}  // namespace
}  // namespace url_utils
}  // namespace autofill_assistant
