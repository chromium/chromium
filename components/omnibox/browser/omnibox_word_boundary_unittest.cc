// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_word_boundary.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

TEST(OmniboxWordBoundaryTest, GetDeletionBoundary_Backward) {
  EXPECT_EQ(7, GetDeletionBoundary(u"google.com", 10, false));
  EXPECT_EQ(0, GetDeletionBoundary(u"google.com", 7, false));

  EXPECT_EQ(11, GetDeletionBoundary(u"google.com/path", 15, false));
  EXPECT_EQ(7, GetDeletionBoundary(u"google.com/path", 11, false));

  EXPECT_EQ(0, GetDeletionBoundary(u"https://google.com", 8, false));

  EXPECT_EQ(11, GetDeletionBoundary(u"google.com:8080", 15, false));

  EXPECT_EQ(11, GetDeletionBoundary(u"google.com/path_/file.html", 17, false));

  EXPECT_EQ(11, GetDeletionBoundary(u"[2001:db8::1]", 13, false));
  EXPECT_EQ(6, GetDeletionBoundary(u"[2001:db8::1]", 11, false));
}

TEST(OmniboxWordBoundaryTest, GetDeletionBoundary_Forward) {
  EXPECT_EQ(6, GetDeletionBoundary(u"google.com", 0, true));
  EXPECT_EQ(10, GetDeletionBoundary(u"google.com", 6, true));

  EXPECT_EQ(10, GetDeletionBoundary(u"google.com/path", 7, true));
  EXPECT_EQ(15, GetDeletionBoundary(u"google.com/path", 10, true));

  EXPECT_EQ(14, GetDeletionBoundary(u"https://google.com", 5, true));

  EXPECT_EQ(15, GetDeletionBoundary(u"google.com:8080", 10, true));

  EXPECT_EQ(21, GetDeletionBoundary(u"google.com/path_/file.html", 16, true));

  EXPECT_EQ(9, GetDeletionBoundary(u"[2001:db8::1]", 5, true));
}

}  // namespace omnibox
