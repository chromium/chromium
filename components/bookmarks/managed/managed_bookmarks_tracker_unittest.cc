// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmarks_tracker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
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
    auto owned_managed_node =
        std::make_unique<BookmarkPermanentNode>(100, BookmarkNode::FOLDER);
    BookmarkPermanentNode* managed_node = owned_managed_node.get();
    ManagedBookmarksTracker::LoadInitial(
        managed_node, prefs_.GetList(prefs::kManagedBookmarks), 101);
    managed_node->set_visible(!managed_node->children().empty());
    managed_node->SetTitle(l10n_util::GetStringUTF16(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME));

    std::unique_ptr<TestBookmarkClient> client(new TestBookmarkClient);
    client->SetManagedNodeToLoad(std::move(owned_managed_node));
    model_.reset(new BookmarkModel(std::move(client)));

    model_->AddObserver(&observer_);
    EXPECT_CALL(observer_, BookmarkModelLoaded(model_.get(), _));
    model_->Load(&prefs_, scoped_temp_dir_.GetPath(),
                 base::ThreadTaskRunnerHandle::Get(),
                 base::ThreadTaskRunnerHandle::Get());
    test::WaitForBookmarkModelToLoad(model_.get());
    Mock::VerifyAndClearExpectations(&observer_);

    TestBookmarkClient* client_ptr =
        static_cast<TestBookmarkClient*>(model_->client());
    managed_node_ = client_ptr->managed_node();

    managed_bookmarks_tracker_.reset(new ManagedBookmarksTracker(
        model_.get(),
        &prefs_,
        base::Bind(&ManagedBookmarksTrackerTest::GetManagementDomain)));
    managed_bookmarks_tracker_->Init(managed_node_);
  }

  const BookmarkNode* managed_node() {
    return managed_node_;
  }

  bool IsManaged(const BookmarkNode* node) {
    return node && node->HasAncestor(managed_node_);
  }

  static std::unique_ptr<base::DictionaryValue> CreateBookmark(
      const std::string& title,
      const std::string& url) {
    EXPECT_TRUE(GURL(url).is_valid());
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("name", title);
    dict->SetString("url", GURL(url).spec());
    return dict;
  }

  static std::unique_ptr<base::DictionaryValue> CreateFolder(
      const std::string& title,
      std::unique_ptr<base::ListValue> children) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("name", title);
    dict->Set("children", std::move(children));
    return dict;
  }

  static std::unique_ptr<base::ListValue> CreateTestTree() {
    auto folder = std::make_unique<base::ListValue>();
    folder->Append(CreateFolder("Empty", std::make_unique<base::ListValue>()));
    folder->Append(CreateBookmark("Youtube", "http://youtube.com/"));

    auto list = std::make_unique<base::ListValue>();
    list->Append(CreateBookmark("Google", "http://google.com/"));
    list->Append(CreateFolder("Folder", std::move(folder)));

    return list;
  }

  static std::string GetManagementDomain() {
    return std::string();
  }

  static std::string GetManagedFolderTitle() {
    return l10n_util::GetStringUTF8(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
  }

  static std::unique_ptr<base::DictionaryValue> CreateExpectedTree() {
    return CreateFolder(GetManagedFolderTitle(), CreateTestTree());
  }

  static bool NodeMatchesValue(const BookmarkNode* node,
                               const base::DictionaryValue* dict) {
    base::string16 title;
    if (!dict->GetString("name", &title) || node->GetTitle() != title)
      return false;

    if (node->is_folder()) {
      const base::ListValue* children = nullptr;
      if (!dict->GetList("children", &children) ||
          node->children().size() != children->GetSize()) {
        return false;
      }
      size_t i = 0;
      return std::all_of(node->children().cbegin(), node->children().cend(),
                         [children, &i](const auto& child_node) {
                           const base::DictionaryValue* child = nullptr;
                           return children->GetDictionary(i++, &child) &&
                                  NodeMatchesValue(child_node.get(), child);
                         });
    }
    if (!node->is_url())
      return false;
    std::string url;
    return dict->GetString("url", &url) && node->url() == url;
  }

  base::ScopedTempDir scoped_temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<BookmarkModel> model_;
  MockBookmarkModelObserver observer_;
  BookmarkPermanentNode* managed_node_;
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
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_FALSE(managed_node()->children().empty());
  EXPECT_TRUE(managed_node()->IsVisible());

  std::unique_ptr<base::DictionaryValue> expected(CreateExpectedTree());
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, LoadInitialWithTitle) {
  // Set the managed folder title.
  const char kExpectedFolderName[] = "foo";
  prefs_.SetString(prefs::kManagedBookmarksFolderName, kExpectedFolderName);
  // Set a policy before loading the model.
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model_->other_node()->children().empty());
  EXPECT_FALSE(managed_node()->children().empty());
  EXPECT_TRUE(managed_node()->IsVisible());

  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(kExpectedFolderName, CreateTestTree()));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, SwapNodes) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Swap the Google bookmark with the Folder.
  std::unique_ptr<base::ListValue> updated(CreateTestTree());
  std::unique_ptr<base::Value> removed;
  ASSERT_TRUE(updated->Remove(0, &removed));
  updated->Append(std::move(removed));

  // These two nodes should just be swapped.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeMoved(model_.get(), parent, 1, parent, 0));
  prefs_.SetManagedPref(prefs::kManagedBookmarks, updated->CreateDeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveNode) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Remove the Folder.
  std::unique_ptr<base::ListValue> updated(CreateTestTree());
  ASSERT_TRUE(updated->Remove(1, nullptr));

  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_.get(), parent, 1, _, _));
  prefs_.SetManagedPref(prefs::kManagedBookmarks, updated->CreateDeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, CreateNewNodes) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Put all the nodes inside another folder.
  std::unique_ptr<base::ListValue> updated(new base::ListValue);
  updated->Append(CreateFolder("Container", CreateTestTree()));

  EXPECT_CALL(observer_, BookmarkNodeAdded(model_.get(), _, _)).Times(5);
  // The remaining nodes have been pushed to positions 1 and 2; they'll both be
  // removed when at position 1.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_.get(), parent, 1, _, _))
      .Times(2);
  prefs_.SetManagedPref(prefs::kManagedBookmarks, updated->CreateDeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveAll) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(managed_node()->IsVisible());

  // Remove the policy.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_.get(), parent, 0, _, _))
      .Times(2);
  prefs_.RemoveManagedPref(prefs::kManagedBookmarks);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(managed_node()->children().empty());
  EXPECT_FALSE(managed_node()->IsVisible());
}

TEST_F(ManagedBookmarksTrackerTest, IsManaged) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
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
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_EQ(2u, managed_node()->children().size());

  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_.get(), model_->bookmark_bar_node(), 0));
  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_.get(), model_->bookmark_bar_node(), 1));
  model_->AddURL(model_->bookmark_bar_node(),
                 0,
                 base::ASCIIToUTF16("Test"),
                 GURL("http://google.com/"));
  model_->AddFolder(
      model_->bookmark_bar_node(), 1, base::ASCIIToUTF16("Test Folder"));
  EXPECT_EQ(2u, model_->bookmark_bar_node()->children().size());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, BookmarkAllUserNodesRemoved(model_.get(), _));
  model_->RemoveAllUserBookmarks();
  EXPECT_EQ(2u, managed_node()->children().size());
  EXPECT_EQ(0u, model_->bookmark_bar_node()->children().size());
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace bookmarks
