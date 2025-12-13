// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/position.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsApiPositionTest, Comparison) {
  ASSERT_TRUE(Position(0, NodeId(NodeId::Type::kContent, "aaa")) ==
              Position(0, NodeId(NodeId::Type::kContent, "aaa")));
  ASSERT_TRUE(Position(0, NodeId(NodeId::Type::kContent, "aaa")) !=
              Position(1, NodeId(NodeId::Type::kContent, "aaa")));
  ASSERT_TRUE(Position(0, NodeId(NodeId::Type::kContent, "aaa")) !=
              Position(0, NodeId(NodeId::Type::kCollection, "aaa")));
}

}  // namespace
}  // namespace tabs_api
