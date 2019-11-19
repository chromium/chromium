// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_expanded_state_tracker.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

class BookmarkExpandedStateTrackerTest : public testing::Test {
 public:
  BookmarkExpandedStateTrackerTest();
  ~BookmarkExpandedStateTrackerTest() override;

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<BookmarkModel> model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExpandedStateTrackerTest);
};

BookmarkExpandedStateTrackerTest::BookmarkExpandedStateTrackerTest() {}

BookmarkExpandedStateTrackerTest::~BookmarkExpandedStateTrackerTest() {}

void BookmarkExpandedStateTrackerTest::SetUp() {
  prefs_.registry()->RegisterListPref(prefs::kBookmarkEditorExpandedNodes);
  prefs_.registry()->RegisterListPref(prefs::kManagedBookmarks);
  model_.reset(new BookmarkModel(std::make_unique<TestBookmarkClient>()));
  model_->Load(&prefs_, base::FilePath(),
               base::ThreadTaskRunnerHandle::Get(),
               base::ThreadTaskRunnerHandle::Get());
  test::WaitForBookmarkModelToLoad(model_.get());
}

void BookmarkExpandedStateTrackerTest::TearDown() {
  model_.reset();
  base::RunLoop().RunUntilIdle();
}

// Various assertions for SetExpandedNodes.
TEST_F(BookmarkExpandedStateTrackerTest, SetExpandedNodes) {
  BookmarkExpandedStateTracker* tracker = model_->expanded_state_tracker();

  // Should start out initially empty.
  EXPECT_TRUE(tracker->GetExpandedNodes().empty());

  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(model_->bookmark_bar_node());
  tracker->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());

  // Add a folder and mark it expanded.
  const BookmarkNode* n1 = model_->AddFolder(
      model_->bookmark_bar_node(), 0, base::ASCIIToUTF16("x"));
  nodes.insert(n1);
  tracker->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());

  // Remove the folder, which should remove it from the list of expanded nodes.
  model_->Remove(model_->bookmark_bar_node()->children().front().get());
  nodes.erase(n1);
  n1 = nullptr;
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());
}

TEST_F(BookmarkExpandedStateTrackerTest, RemoveAllUserBookmarks) {
  BookmarkExpandedStateTracker* tracker = model_->expanded_state_tracker();

  // Add a folder and mark it expanded.
  const BookmarkNode* n1 = model_->AddFolder(
      model_->bookmark_bar_node(), 0, base::ASCIIToUTF16("x"));
  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(n1);
  tracker->SetExpandedNodes(nodes);
  // Verify that the node is present.
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());
  // Call remove all.
  model_->RemoveAllUserBookmarks();
  // Verify node is not present.
  EXPECT_TRUE(tracker->GetExpandedNodes().empty());
}

}  // namespace bookmarks
