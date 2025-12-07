// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/task_manager/task_manager_table_model.h"

#include <string>
#include <string_view>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

using TaskManagerTableModelTest = testing::Test;

namespace {
bool ContainsSubstringsInOrder(std::u16string_view text,
                               const std::vector<std::u16string>& substrings) {
  for (const auto& substring : substrings) {
    size_t found_pos = text.find(substring);
    if (found_pos == std::u16string_view::npos) {
      return false;
    }
    text.remove_prefix(found_pos + substring.length());
  }
  return true;
}
}  // namespace

TEST_F(TaskManagerTableModelTest, FormatListToString) {
  std::vector<std::u16string> tasks;

  EXPECT_EQ(task_manager::TaskManagerTableModel::FormatListToString(tasks),
            std::u16string());

  tasks.push_back(u"task1");
  EXPECT_TRUE(ContainsSubstringsInOrder(
      task_manager::TaskManagerTableModel::FormatListToString(tasks), tasks));
  tasks.push_back(u"task2");
  EXPECT_TRUE(ContainsSubstringsInOrder(
      task_manager::TaskManagerTableModel::FormatListToString(tasks), tasks));
  tasks.push_back(u"task3");
  EXPECT_TRUE(ContainsSubstringsInOrder(
      task_manager::TaskManagerTableModel::FormatListToString(tasks), tasks));
}

TEST_F(TaskManagerTableModelTest, DefaultCategory) {
  // Prevents a situation where a developer creates a new category, and sets the
  // new kDefaultCategory to the new category, without updating kMax.
  EXPECT_LE(
      static_cast<int>(task_manager::TaskManagerTableModel::kDefaultCategory),
      static_cast<int>(task_manager::DisplayCategory::kMax));
}
