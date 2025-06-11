// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/node_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsApiNodeIdTest, Comparison) {
  ASSERT_TRUE(NodeId(NodeId::Type::kContent, "aaa") ==
              NodeId(NodeId::Type::kContent, "aaa"));

  ASSERT_TRUE(NodeId(NodeId::Type::kContent, "aaa") !=
              NodeId(NodeId::Type::kCollection, "aaa"));

  ASSERT_TRUE(NodeId(NodeId::Type::kCollection, "aaa") !=
              NodeId(NodeId::Type::kCollection, "bbb"));

  ASSERT_TRUE(NodeId(NodeId::Type::kContent, "aaa") !=
              NodeId(NodeId::Type::kCollection, "bbb"));
}

}  // namespace
}  // namespace tabs_api
