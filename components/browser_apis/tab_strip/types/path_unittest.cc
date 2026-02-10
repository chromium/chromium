// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/path.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsApiPathTest, Comparison) {
  NodeId id1(NodeId::Type::kCollection, "aaa");
  NodeId id2(NodeId::Type::kCollection, "bbb");

  Path path1({id1, id2});
  Path path2({id1, id2});
  Path path3({id1});
  Path path4({id2, id1});

  EXPECT_EQ(path1, path2);
  EXPECT_NE(path1, path3);
  EXPECT_NE(path1, path4);
}

}  // namespace
}  // namespace tabs_api
