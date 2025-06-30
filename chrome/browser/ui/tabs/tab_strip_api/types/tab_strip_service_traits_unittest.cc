// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/node_id_traits.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/position_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsStripServiceMojoTraitsTest, ConvertNodeId) {
  NodeId original(NodeId::Type::kCollection, "super_secret_id");

  auto serialized = mojom::NodeId::Serialize(&original);

  NodeId deserialized;
  mojom::NodeId::Deserialize(serialized, &deserialized);

  ASSERT_TRUE(original == deserialized);
}

TEST(TabsStripServiceMojoTraitsTest, ConvertPosition) {
  Position original(0, NodeId(NodeId::Type::kCollection, "super_secret_id"));

  auto serialized = mojom::Position::Serialize(&original);

  Position deserialized;
  mojom::Position::Deserialize(serialized, &deserialized);

  ASSERT_TRUE(original == deserialized);
}

}  // namespace
}  // namespace tabs_api
