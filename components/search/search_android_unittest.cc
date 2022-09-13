// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace search {

namespace {

TEST(SearchTest, EmbeddedSearchAPIEnabled) {
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
}

}  // namespace

}  // namespace chrome
