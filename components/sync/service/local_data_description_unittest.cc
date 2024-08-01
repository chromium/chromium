// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/local_data_description.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace syncer {
namespace {

// Convenience helper, since LocalDataDescription doesn't support designated
// inits.
LocalDataDescription BuildDescription(int item_count,
                                      const std::vector<std::string>& domains,
                                      int domain_count) {
  LocalDataDescription description;
  description.item_count = item_count;
  description.domains = domains;
  description.domain_count = domain_count;
  return description;
}

TEST(LocalDataDescriptionTest, AtMostThreeDomains) {
  EXPECT_EQ(LocalDataDescription({GURL("http://a.com")}),
            BuildDescription(1, {"a.com"}, 1));
  EXPECT_EQ(LocalDataDescription({
                GURL("http://a.com"),
                GURL("http://b.com"),
            }),
            BuildDescription(2, {"a.com", "b.com"}, 2));
  EXPECT_EQ(LocalDataDescription({
                GURL("http://a.com"),
                GURL("http://b.com"),
                GURL("http://c.com"),
            }),
            BuildDescription(3, {"a.com", "b.com", "c.com"}, 3));
  // d.com is not included.
  EXPECT_EQ(LocalDataDescription({
                GURL("http://a.com"),
                GURL("http://b.com"),
                GURL("http://c.com"),
                GURL("http://d.com"),
            }),
            BuildDescription(4, {"a.com", "b.com", "c.com"}, 4));
}

TEST(LocalDataDescriptionTest, DomainsAreSorted) {
  EXPECT_EQ(LocalDataDescription({GURL("http://c.com"), GURL("http://b.com"),
                                  GURL("http://a.com")}),
            BuildDescription(3, {"a.com", "b.com", "c.com"}, 3));
  // Sorting shouldn't take the scheme into account, http://b.com is < than
  // https://a.com but a.com < b.com.
  EXPECT_EQ(LocalDataDescription({GURL("http://b.com"), GURL("https://a.com")}),
            BuildDescription(2, {"a.com", "b.com"}, 2));
}

TEST(LocalDataDescriptionTest, DomainsAreDeduped) {
  EXPECT_EQ(LocalDataDescription({GURL("http://a.com"), GURL("https://a.com"),
                                  GURL("https://a.com/foo")}),
            BuildDescription(3, {"a.com"}, 1));
}

TEST(LocalDataDescriptionTest, GetDomainsDisplayText) {
  EXPECT_EQ(GetDomainsDisplayText(LocalDataDescription({GURL("http://a.com")})),
            u"a.com");
  EXPECT_EQ(GetDomainsDisplayText(LocalDataDescription(
                {GURL("http://a.com"), GURL("http://b.com")})),
            u"a.com, b.com");
  EXPECT_EQ(
      GetDomainsDisplayText(LocalDataDescription(
          {GURL("http://a.com"), GURL("http://b.com"), GURL("http://c.com")})),
      u"a.com, b.com, and 1 more");
  EXPECT_EQ(GetDomainsDisplayText(LocalDataDescription(
                {GURL("http://a.com"), GURL("http://b.com"),
                 GURL("http://c.com"), GURL("http://d.com")})),
            u"a.com, b.com, and 2 more");
  EXPECT_EQ(
      GetDomainsDisplayText(LocalDataDescription(
          {GURL("http://a.com"), GURL("http://a.com"), GURL("http://b.com")})),
      u"a.com, b.com");
}

}  // namespace
}  // namespace syncer
