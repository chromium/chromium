// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/navigation_task_id.h"

#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

class NavigationTaskIDTest : public testing::Test {
 public:
  NavigationTaskIDTest() = default;

  NavigationTaskIDTest(const NavigationTaskIDTest&) = delete;
  NavigationTaskIDTest& operator=(const NavigationTaskIDTest&) = delete;

  ~NavigationTaskIDTest() override = default;

  void SetUp() override {
    navigation_entry_ = content::NavigationEntry::Create();
  }

 protected:
  std::unique_ptr<content::NavigationEntry> navigation_entry_;
};

TEST_F(NavigationTaskIDTest, TaskIDTest) {
  NavigationTaskId* navigation_task_id =
      NavigationTaskId::Get(navigation_entry_.get());
  navigation_task_id->set_id(test_data::kTaskId);
  navigation_task_id->set_parent_id(test_data::kParentTaskId);
  navigation_task_id->set_root_id(test_data::kRootTaskId);

  EXPECT_EQ(test_data::kTaskId,
            NavigationTaskId::Get(navigation_entry_.get())->id());
  EXPECT_EQ(test_data::kParentTaskId,
            NavigationTaskId::Get(navigation_entry_.get())->parent_id());
  EXPECT_EQ(test_data::kRootTaskId,
            NavigationTaskId::Get(navigation_entry_.get())->root_id());

  NavigationTaskId cloned_navigation_task_id(*navigation_task_id);

  EXPECT_EQ(test_data::kTaskId, cloned_navigation_task_id.id());
  EXPECT_EQ(test_data::kParentTaskId, cloned_navigation_task_id.parent_id());
  EXPECT_EQ(test_data::kRootTaskId, cloned_navigation_task_id.root_id());
}

}  // namespace sessions
