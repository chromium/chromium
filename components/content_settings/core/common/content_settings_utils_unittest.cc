// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

TEST(ContentSettingsUtilsTest, URLToSchemefulSitePattern) {
  // Only uses the eTLD+1 (aka registrable domain)
  EXPECT_EQ(
      "http://[*.]google.com",
      URLToSchemefulSitePattern(GURL("http://mail.google.com")).ToString());
  EXPECT_EQ("http://[*.]google.com",
            URLToSchemefulSitePattern(GURL("http://www.foo.mail.google.com"))
                .ToString());
  EXPECT_EQ("http://[*.]google.com",
            URLToSchemefulSitePattern(GURL("http://google.com")).ToString());

  // Includes the (right) scheme
  EXPECT_EQ("http://[*.]google.com",
            URLToSchemefulSitePattern(GURL("http://google.com")).ToString());
  EXPECT_EQ("https://[*.]google.com",
            URLToSchemefulSitePattern(GURL("https://google.com")).ToString());

  // Strips the port
  EXPECT_EQ(
      "http://[*.]google.com",
      URLToSchemefulSitePattern(GURL("http://google.com:3000")).ToString());
  EXPECT_EQ("http://[*.]google.com",
            URLToSchemefulSitePattern(GURL("http://google.com:80")).ToString());
  EXPECT_EQ(
      "https://[*.]google.com",
      URLToSchemefulSitePattern(GURL("https://google.com:443")).ToString());

  // Strips the path
  EXPECT_EQ(
      "http://[*.]google.com",
      URLToSchemefulSitePattern(GURL("http://google.com/example/")).ToString());
  EXPECT_EQ(
      "http://[*.]google.com",
      URLToSchemefulSitePattern(GURL("http://google.com/example/example.html"))
          .ToString());

  // Opaque origins shouldn't match anything.
  EXPECT_EQ("", URLToSchemefulSitePattern(
                    GURL("data:text/html,<body>Hello World</body>"))
                    .ToString());

  // This should mirror SchemefulSite which considers file URLs
  // equal, ignoring the path.
  EXPECT_EQ("file:///*",
            URLToSchemefulSitePattern(GURL("file:///foo/bar.html")).ToString());

  EXPECT_EQ(
      "https://127.0.0.1",
      URLToSchemefulSitePattern(GURL("https://127.0.0.1:8080")).ToString());
  EXPECT_EQ("https://[::1]",
            URLToSchemefulSitePattern(GURL("https://[::1]:8080")).ToString());

  EXPECT_EQ(
      "https://localhost",
      URLToSchemefulSitePattern(GURL("https://localhost:3000")).ToString());

  // Invalid patterns
  EXPECT_FALSE(
      URLToSchemefulSitePattern(GURL("invalid://test:3000")).IsValid());
  EXPECT_FALSE(
      URLToSchemefulSitePattern(GURL("invalid://test.com/path")).IsValid());

  // URL patterns that are not currently matched
  EXPECT_EQ("", URLToSchemefulSitePattern(
                    GURL("filesystem:http://www.google.com/temporary/"))
                    .ToString());
  EXPECT_EQ("", URLToSchemefulSitePattern(GURL("chrome://test")).ToString());
  EXPECT_EQ("",
            URLToSchemefulSitePattern(GURL("devtools://devtools/")).ToString());
}

}  // namespace
}  // namespace content_settings
