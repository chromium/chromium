// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_id_utils.h"

#include "components/browser_apis/tab_strip/types/node_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api::utils {

TEST(TabStripApiUtilsTest, CheckPath_Empty) {
  auto result = CheckPath(Path(), NodeId(NodeId::Type::kWindow, "1"),
                          NodeId(NodeId::Type::kCollection, "123"));
  ASSERT_TRUE(result.has_value());
}

TEST(TabStripApiUtilsTest, CheckPath_Valid) {
  NodeId window_id(NodeId::Type::kWindow, "1");
  NodeId tab_strip_id(NodeId::Type::kCollection, "123");
  std::vector<NodeId> components;
  components.emplace_back(window_id);
  components.emplace_back(tab_strip_id);
  Path path(std::move(components));

  auto result = CheckPath(path, window_id, tab_strip_id);
  ASSERT_TRUE(result.has_value());
}

TEST(TabStripApiUtilsTest, CheckPath_WrongTabStrip) {
  NodeId window_id(NodeId::Type::kWindow, "1");
  NodeId tab_strip_id(NodeId::Type::kCollection, "123");
  NodeId other_tab_strip_id(NodeId::Type::kCollection, "456");
  std::vector<NodeId> components;
  components.emplace_back(window_id);
  components.emplace_back(other_tab_strip_id);
  Path path(std::move(components));

  auto result = CheckPath(path, window_id, tab_strip_id);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}

TEST(TabStripApiUtilsTest, CheckPath_WrongWindow) {
  NodeId window_id(NodeId::Type::kWindow, "1");
  NodeId other_window_id(NodeId::Type::kWindow, "2");
  NodeId tab_strip_id(NodeId::Type::kCollection, "123");
  std::vector<NodeId> components;
  components.emplace_back(other_window_id);
  components.emplace_back(tab_strip_id);
  Path path(std::move(components));

  auto result = CheckPath(path, window_id, tab_strip_id);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}

TEST(TabStripApiUtilsTest, CheckIsContentType) {
  auto result = CheckIsContentType(NodeId(NodeId::Type::kContent, "123"));
  ASSERT_TRUE(result.has_value());
}

TEST(TabStripApiUtilsTest, CheckIsContentType_WrongType) {
  auto result = CheckIsContentType(NodeId(NodeId::Type::kInvalid, "123"));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}

TEST(TabStripApiUtilsTest, GetNativeTabId) {
  auto result = GetNativeTabId(NodeId(NodeId::Type::kContent, "123"));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(123, result.value());
}

TEST(TabStripApiUtilsTest, GetNativeTabId_BadType) {
  auto result = GetNativeTabId(NodeId(NodeId::Type::kContent, "abc"));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}

TEST(TabStripApiUtilsTest, GetContentNativeTabId) {
  auto result = GetContentNativeTabId(NodeId(NodeId::Type::kContent, "123"));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(123, result.value());
}

TEST(TabStripApiUtilsTest, GetContentNativeTabId_Invalid) {
  auto result = GetContentNativeTabId(NodeId(NodeId::Type::kInvalid, "123"));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}
}  // namespace tabs_api::utils
