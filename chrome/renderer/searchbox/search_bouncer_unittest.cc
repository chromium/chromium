// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/search_bouncer.h"

#include <vector>

#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SearchBouncerTest : public testing::Test {
 public:
  void SetUp() override {
    bouncer_.SetNewTabPageURL(GURL("http://example.com/newtab"));
  }

  SearchBouncer bouncer_;
};

TEST_F(SearchBouncerTest, IsNewTabPage) {
  EXPECT_FALSE(bouncer_.IsNewTabPage(GURL("http://example.com/foo")));
  EXPECT_TRUE(bouncer_.IsNewTabPage(GURL("http://example.com/newtab")));
  EXPECT_TRUE(bouncer_.IsNewTabPage(GURL("http://example.com/newtab?q=foo")));
  EXPECT_TRUE(bouncer_.IsNewTabPage(GURL("http://example.com/newtab#q=foo")));
  EXPECT_TRUE(
      bouncer_.IsNewTabPage(GURL("http://example.com/newtab#q=foo?q=foo")));
}
