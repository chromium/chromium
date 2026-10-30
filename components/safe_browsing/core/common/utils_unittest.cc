// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/utils.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

TEST(SafeBrowsingUtilsTest, ShortURLForReportingNonDataUrls) {
  EXPECT_EQ(ShortURLForReporting(GURL()), "");
  EXPECT_EQ(ShortURLForReporting(GURL("https://example.com/path?q=1")),
            "https://example.com/path?q=1");
  // data: URLs without a comma or with an empty payload are left unchanged.
  EXPECT_EQ(ShortURLForReporting(GURL("data:text/plain;base64")),
            "data:text/plain;base64");
  EXPECT_EQ(ShortURLForReporting(GURL("data:text/plain,")), "data:text/plain,");
}

TEST(SafeBrowsingUtilsTest, ShortURLForReportingDataUrl) {
  // The payload is replaced by the hex SHA-256 of the full URL spec.
  EXPECT_EQ(ShortURLForReporting(GURL("data:text/plain,hello")),
            "data:text/plain,"
            "E5DD724E20159BCB2C3DCF70ECA829E7B39DECB6C72BEFF5AA37AE885DF61F51");
}

TEST(SafeBrowsingUtilsTest, ShortURLForReportingDoesNotRetainPayloadCapacity) {
  constexpr std::string_view kPrefix = "data:text/plain;base64,";
  const GURL data_url(base::StrCat({kPrefix, std::string(100 * 1024, 'A')}));
  ASSERT_TRUE(data_url.is_valid());

  std::string shortened = ShortURLForReporting(data_url);
  EXPECT_EQ(shortened.size(), kPrefix.size() + 64u);
  EXPECT_TRUE(shortened.starts_with(kPrefix));
  // The result is moved into long-lived protos, so it must not keep the
  // allocation of the original 100 KiB payload.
  EXPECT_LT(shortened.capacity(), 256u);
}

}  // namespace safe_browsing
