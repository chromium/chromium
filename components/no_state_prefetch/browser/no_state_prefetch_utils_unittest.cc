// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace prerender {

// Ensure that we detect GWS origin URLs correctly.
TEST(NoStatePrefetchUtilsTest, DetectGWSOriginURLTest) {
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://www.google.com/#asdf")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("https://www.google.com")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://www.google.com/?a=b")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://www.google.com/search?q=hi")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://google.com")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://WWW.GooGLE.CoM")));
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://www.google.co.uk")));
  // Non-standard ports are allowed for integration tests with the embedded
  // server.
  EXPECT_TRUE(IsGoogleOriginURL(GURL("http://www.google.com:42/")));

  EXPECT_FALSE(IsGoogleOriginURL(GURL("http://news.google.com")));
  EXPECT_FALSE(IsGoogleOriginURL(GURL("http://www.chromium.org")));
  EXPECT_FALSE(IsGoogleOriginURL(GURL("what://www.google.com")));
}

}  // namespace prerender
