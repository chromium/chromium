// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/node_id.h"

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

TEST(TabsApiNodeIdTest, ToTabHandle) {
  const NodeId content_node(NodeId::Type::kContent, "123");
  // Valid conversation from NodeId to TabHandle.
  const std::optional<tabs::TabHandle> tab_handle = content_node.ToTabHandle();
  ASSERT_TRUE(tab_handle.has_value());
  EXPECT_EQ(123, tab_handle->raw_value());

  // Invalid because of an incorrect type.
  const NodeId collection_node(NodeId::Type::kCollection, "123");
  EXPECT_FALSE(collection_node.ToTabHandle().has_value());
  const NodeId invalid_type_node(NodeId::Type::kInvalid, "123");
  EXPECT_FALSE(invalid_type_node.ToTabHandle().has_value());

  // Invalid because of an incorrect id.
  const NodeId invalid_id_node(NodeId::Type::kContent, "abc");
  EXPECT_FALSE(invalid_id_node.ToTabHandle().has_value());
  const NodeId empty_id_node(NodeId::Type::kContent, "");
  EXPECT_FALSE(empty_id_node.ToTabHandle().has_value());
}

TEST(TabsApiNodeIdTest, ToTabCollectionHandle) {
  // Valid conversation from NodeId to TabCollection.
  const NodeId collection_node(NodeId::Type::kCollection, "456");
  const std::optional<tabs::TabCollectionHandle> collection_handle =
      collection_node.ToTabCollectionHandle();
  ASSERT_TRUE(collection_handle.has_value());
  EXPECT_EQ(456, collection_handle->raw_value());

  // Invalid because of an incorrect type.
  const NodeId content_node(NodeId::Type::kContent, "456");
  EXPECT_FALSE(content_node.ToTabCollectionHandle().has_value());
  const NodeId invalid_type_node(NodeId::Type::kInvalid, "456");
  EXPECT_FALSE(invalid_type_node.ToTabCollectionHandle().has_value());

  // Invalid because of an incorrect id.
  const NodeId invalid_id_node(NodeId::Type::kCollection, "xyz");
  EXPECT_FALSE(invalid_id_node.ToTabCollectionHandle().has_value());
  const NodeId empty_id_node(NodeId::Type::kCollection, "");
  EXPECT_FALSE(empty_id_node.ToTabCollectionHandle().has_value());
}

}  // namespace
}  // namespace tabs_api
