// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/position.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsApiPositionTest, Comparison) {
  Path path_a({NodeId(NodeId::Type::kCollection, "aaa")});
  Path path_b({NodeId(NodeId::Type::kCollection, "bbb")});

  ASSERT_TRUE(Position(0, path_a) == Position(0, path_a));
  ASSERT_TRUE(Position(0, path_a) != Position(1, path_a));
  ASSERT_TRUE(Position(0, path_a) != Position(0, path_b));
}

}  // namespace
}  // namespace tabs_api
