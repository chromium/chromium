// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_item_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

TEST(OfflinePageItemUtils, EqualsIgnoringFragment) {
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/"),
                                     GURL("http://example.com/")));
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/"),
                                     GURL("http://example.com/#test")));
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/#test"),
                                     GURL("http://example.com/")));
  EXPECT_TRUE(EqualsIgnoringFragment(GURL("http://example.com/#test"),
                                     GURL("http://example.com/#test2")));
  EXPECT_FALSE(EqualsIgnoringFragment(GURL("http://example.com/"),
                                      GURL("http://test.com/#test")));
}

}  // namespace
}  // namespace offline_pages
