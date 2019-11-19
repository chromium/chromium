// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/article_entry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

TEST(DomDistillerArticleEntryTest, TestIsEntryValid) {
  ArticleEntry entry;
  EXPECT_FALSE(IsEntryValid(entry));
  entry.entry_id = "entry0";
  EXPECT_TRUE(IsEntryValid(entry));
  entry.pages.push_back(GURL());
  EXPECT_FALSE(IsEntryValid(entry));
  entry.pages.back() = GURL("https://example.com/1");
  EXPECT_TRUE(IsEntryValid(entry));
}

}  // namespace dom_distiller
