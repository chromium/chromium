// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/url_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace url_utils {
namespace {

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

}  // namespace
}  // namespace url_utils
}  // namespace autofill_assistant
