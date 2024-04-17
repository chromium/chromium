// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmarks_tracker.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::Mock;
using testing::_;

namespace bookmarks {

class ManagedBookmarksTrackerTest : public testing::Test {
 public:
  ManagedBookmarksTrackerTest() : managed_node_(nullptr) {}
  ~ManagedBookmarksTrackerTest() override {}

  void SetUp() override {
    RegisterManagedBookmarksPrefs(prefs_.registry());
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    if (model_)
      model_->RemoveObserver(&observer_);
    base::RunLoop().RunUntilIdle();
  }

  void CreateModel() {
    // Simulate the creation of the managed node by the BookmarkClient.
    auto client = std::make_unique<TestBookmarkClient>();
    managed_node_ = client->EnableManagedNode();
    ManagedBookmarksTracker::LoadInitial(
        managed_node_, prefs_.GetList(prefs::kManagedBookmarks), 101);
    managed_node_->SetTitle(l10n_util::GetStringUTF16(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME));

    model_ = std::make_unique<BookmarkModel>(std::move(client));

    model_->AddObserver(&observer_);
    EXPECT_CALL(observer_, BookmarkModelLoaded(_));
    model_->Load(scoped_temp_dir_.GetPath());
    test::WaitForBookmarkModelToLoad(model_.get());
    Mock::VerifyAndClearExpectations(&observer_);

    managed_bookmarks_tracker_ = std::make_unique<ManagedBookmarksTracker>(
        model_.get(), &prefs_,
        base::BindRepeating(&ManagedBookmarksTrackerTest::GetManagementDomain));
    managed_bookmarks_tracker_->Init(managed_node_);
  }

  const BookmarkNode* managed_node() {
    return managed_node_;
  }

  bool IsManaged(const BookmarkNode* node) {
    return node && node->HasAncestor(managed_node_.get());
  }

  void SetManagedPref(const std::string& path, const base::Value::List& list) {
    prefs_.SetManagedPref(path, base::Value(list.Clone()));
  }

  static base::Value::Dict CreateBookmark(const std::string& title,
                                          const std::string& url) {
    EXPECT_TRUE(GURL(url).is_valid());
    base::Value::Dict dict;
    dict.Set("name", title);
    dict.Set("url", GURL(url).spec());
    return dict;
  }

  static base::Value::Dict CreateFolder(const std::string& title,
                                        base::Value::List children) {
    base::Value::Dict dict;
    dict.Set("name", title);
    dict.Set("children", std::move(children));
    return dict;
  }

  static base::Value::List CreateTestTree() {
    base::Value::List folder;
    folder.Append(CreateFolder("Empty", base::Value::List()));
    folder.Append(CreateBookmark("Youtube", "http://youtube.com/"));

    base::Value::List list;
    list.Append(CreateBookmark("Google", "http://google.com/"));
    list.Append(CreateFolder("Folder", std::move(folder)));

    return list;
  }

  static std::string GetManagementDomain() {
    return std::string();
  }

  static std::string GetManagedFolderTitle() {
    return l10n_util::GetStringUTF8(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
  }

  static base::Value::Dict CreateExpectedTree() {
    return CreateFolder(GetManagedFolderTitle(), CreateTestTree());
  }

  static bool NodeMatchesValue(const BookmarkNode* node,
                               const base::Value::Dict& dict) {
    const std::string* title = dict.FindString("name");
    if (!title || node->GetTitle() != base::UTF8ToUTF16(*title))
      return false;

    if (node->is_folder()) {
      const base::Value::List* children = dict.FindList("children");
      return children &&
             base::ranges::equal(
                 *children, node->children(),
                 [](const base::Value& child, const auto& child_node) {
                   return child.is_dict() &&
                          NodeMatchesValue(child_node.get(), child.GetDict());
                 });
    }
    if (!node->is_url())
      return false;
    const std::string* url = dict.FindString("url");
    return url && node->url() == *url;
  }

  base::ScopedTempDir scoped_temp_dir_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<BookmarkModel> model_;
  MockBookmarkModelObserver observer_;
  raw_ptr<BookmarkPermanentNode> managed_node_;
  std::unique_ptr<ManagedBookmarksTracker> managed_bookmarks_tracker_;
};

TEST_F(ManagedBookmarksTrackerTest, Empty) {
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_TRUE(managed_node()->children().empty());
  EXPECT_FALSE(managed_node()->IsVisible());
}

TEST_F(ManagedBookmarksTrackerTest, LoadInitial) {
  // Set a policy before loading the model.
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_FALSE(managed_node()->children().empty());
  EXPECT_TRUE(managed_node()->IsVisible());

  base::Value::Dict expected(CreateExpectedTree());
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));
}

TEST_F(ManagedBookmarksTrackerTest, LoadInitialWithTitle) {
  // Set the managed folder title.
  const char kExpectedFolderName[] = "foo";
  prefs_.SetString(prefs::kManagedBookmarksFolderName, kExpectedFolderName);
  // Set a policy before loading the model.
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_FALSE(managed_node()->children().empty());
  EXPECT_TRUE(managed_node()->IsVisible());

  base::Value::Dict expected(
      CreateFolder(kExpectedFolderName, CreateTestTree()));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));
}

TEST_F(ManagedBookmarksTrackerTest, DynamicRefreshOfTitle) {
  // Set the managed folder title.
  const char kExpectedFolderName[] = "foo";
  prefs_.SetString(prefs::kManagedBookmarksFolderName, kExpectedFolderName);
  // Set a policy before loading the model.
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_FALSE(managed_node()->children().empty());
  EXPECT_TRUE(managed_node()->IsVisible());

  base::Value::Dict expected(
      CreateFolder(kExpectedFolderName, CreateTestTree()));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));

  // Set new managed folder title.
  const char kNewFolderName[] = "bar";
  prefs_.SetString(prefs::kManagedBookmarksFolderName, kNewFolderName);
  expected = CreateFolder(kNewFolderName, CreateTestTree());
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));
}

TEST_F(ManagedBookmarksTrackerTest, SwapNodes) {
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Swap the Google bookmark with the Folder.
  base::Value::List updated(CreateTestTree());
  ASSERT_FALSE(updated.empty());
  base::Value removed = std::move(updated[0]);
  updated.erase(updated.begin());
  updated.Append(std::move(removed));

  // These two nodes should just be swapped.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeMoved(parent, 1, parent, 0));
  SetManagedPref(prefs::kManagedBookmarks, updated);
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  base::Value::Dict expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveNode) {
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Remove the Folder.
  base::Value::List updated(CreateTestTree());
  updated.erase(updated.begin() + 1);

  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(parent, 1, _, _, _));
  SetManagedPref(prefs::kManagedBookmarks, updated);
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  base::Value::Dict expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));
}

TEST_F(ManagedBookmarksTrackerTest, CreateNewNodes) {
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Put all the nodes inside another folder.
  base::Value::List updated;
  updated.Append(CreateFolder("Container", CreateTestTree()));

  EXPECT_CALL(observer_, BookmarkNodeAdded(_, _, _)).Times(5);
  // The remaining nodes have been pushed to positions 1 and 2; they'll both be
  // removed when at position 1.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(parent, 1, _, _, _)).Times(2);
  SetManagedPref(prefs::kManagedBookmarks, updated);
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  base::Value::Dict expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveAll) {
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(managed_node()->IsVisible());

  // Remove the policy.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(parent, 0, _, _, _)).Times(2);
  prefs_.RemoveManagedPref(prefs::kManagedBookmarks);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(managed_node()->children().empty());
  EXPECT_FALSE(managed_node()->IsVisible());
}

TEST_F(ManagedBookmarksTrackerTest, IsManaged) {
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  EXPECT_FALSE(IsManaged(model_->root_node()));
  EXPECT_FALSE(IsManaged(model_->bookmark_bar_node()));
  EXPECT_FALSE(IsManaged(model_->other_node()));
  EXPECT_FALSE(IsManaged(model_->mobile_node()));
  EXPECT_TRUE(IsManaged(managed_node()));

  const BookmarkNode* parent = managed_node();
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_TRUE(IsManaged(parent->children()[0].get()));
  EXPECT_TRUE(IsManaged(parent->children()[1].get()));

  parent = parent->children()[1].get();
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_TRUE(IsManaged(parent->children()[0].get()));
  EXPECT_TRUE(IsManaged(parent->children()[1].get()));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveAllUserBookmarksDoesntRemoveManaged) {
  SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_EQ(2u, managed_node()->children().size());

  EXPECT_CALL(observer_, BookmarkNodeAdded(model_->bookmark_bar_node(), 0, _));
  EXPECT_CALL(observer_, BookmarkNodeAdded(model_->bookmark_bar_node(), 1, _));
  model_->AddURL(model_->bookmark_bar_node(), 0, u"Test",
                 GURL("http://google.com/"));
  model_->AddFolder(model_->bookmark_bar_node(), 1, u"Test Folder");
  EXPECT_EQ(2u, model_->bookmark_bar_node()->children().size());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, BookmarkAllUserNodesRemoved(_, _));
  model_->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_EQ(2u, managed_node()->children().size());
  EXPECT_EQ(0u, model_->bookmark_bar_node()->children().size());
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace bookmarks
