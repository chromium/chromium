// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <stddef.h>
#include <stdint.h>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include <memory>
#include <unordered_set>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_undo_delegate.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon_base/favicon_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;

namespace bookmarks {
namespace {

// Test cases used to test the removal of extra whitespace when adding
// a new folder/bookmark or updating a title of a folder/bookmark.
// Note that whitespace characters are all replaced with spaces, but spaces are
// not collapsed or trimmed.
static struct {
  const std::string input_title;
  const std::string expected_title;
} url_whitespace_test_cases[] = {
  {"foobar", "foobar"},
  // Newlines.
  {"foo\nbar", "foo bar"},
  {"foo\n\nbar", "foo  bar"},
  {"foo\n\n\nbar", "foo   bar"},
  {"foo\r\nbar", "foo  bar"},
  {"foo\r\n\r\nbar", "foo    bar"},
  {"\nfoo\nbar\n", " foo bar "},
  // Spaces should not collapse.
  {"foo  bar", "foo  bar"},
  {" foo bar ", " foo bar "},
  {"  foo  bar  ", "  foo  bar  "},
  // Tabs.
  {"\tfoo\tbar\t", " foo bar "},
  {"\tfoo bar\t", " foo bar "},
  // Mixed cases.
  {"\tfoo\nbar\t", " foo bar "},
  {"\tfoo\r\nbar\t", " foo  bar "},
  {"  foo\tbar\n", "  foo bar "},
  {"\t foo \t  bar  \t", "  foo    bar   "},
  {"\n foo\r\n\tbar\n \t", "  foo   bar   "},
};

// Test cases used to test the removal of extra whitespace when adding
// a new folder/bookmark or updating a title of a folder/bookmark.
static struct {
  const std::string input_title;
  const std::string expected_title;
} title_whitespace_test_cases[] = {
  {"foobar", "foobar"},
  // Newlines.
  {"foo\nbar", "foo bar"},
  {"foo\n\nbar", "foo  bar"},
  {"foo\n\n\nbar", "foo   bar"},
  {"foo\r\nbar", "foo  bar"},
  {"foo\r\n\r\nbar", "foo    bar"},
  {"\nfoo\nbar\n", " foo bar "},
  // Spaces.
  {"foo  bar", "foo  bar"},
  {" foo bar ", " foo bar "},
  {"  foo  bar  ", "  foo  bar  "},
  // Tabs.
  {"\tfoo\tbar\t", " foo bar "},
  {"\tfoo bar\t", " foo bar "},
  // Mixed cases.
  {"\tfoo\nbar\t", " foo bar "},
  {"\tfoo\r\nbar\t", " foo  bar "},
  {"  foo\tbar\n", "  foo bar "},
  {"\t foo \t  bar  \t", "  foo    bar   "},
  {"\n foo\r\n\tbar\n \t", "  foo   bar   "},
};

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

void SwapDateAdded(BookmarkNode* n1, BookmarkNode* n2) {
  Time tmp = n1->date_added();
  n1->set_date_added(n2->date_added());
  n2->set_date_added(tmp);
}

// See comment in PopulateNodeFromString.
using TestNode = ui::TreeNodeWithValue<BookmarkNode::Type>;

// Does the work of PopulateNodeFromString. index gives the index of the current
// element in description to process.
void PopulateNodeImpl(const std::vector<std::string>& description,
                      size_t* index,
                      TestNode* parent) {
  while (*index < description.size()) {
    const std::string& element = description[*index];
    (*index)++;
    if (element == "[") {
      // Create a new folder and recurse to add all the children.
      // Folders are given a unique named by way of an ever increasing integer
      // value. The folders need not have a name, but one is assigned to help
      // in debugging.
      static int next_folder_id = 1;
      TestNode* new_node = parent->Add(std::make_unique<TestNode>(
          base::NumberToString16(next_folder_id++), BookmarkNode::FOLDER));
      PopulateNodeImpl(description, index, new_node);
    } else if (element == "]") {
      // End the current folder.
      return;
    } else {
      // Add a new URL.

      // All tokens must be space separated. If there is a [ or ] in the name it
      // likely means a space was forgotten.
      DCHECK(element.find('[') == std::string::npos);
      DCHECK(element.find(']') == std::string::npos);
      parent->Add(std::make_unique<TestNode>(base::UTF8ToUTF16(element),
                                             BookmarkNode::URL));
    }
  }
}

// Creates and adds nodes to parent based on description. description consists
// of the following tokens (all space separated):
//   [ : creates a new USER_FOLDER node. All elements following the [ until the
//       next balanced ] is encountered are added as children to the node.
//   ] : closes the last folder created by [ so that any further nodes are added
//       to the current folders parent.
//   text: creates a new URL node.
// For example, "a [b] c" creates the following nodes:
//   a 1 c
//     |
//     b
// In words: a node of type URL with the title a, followed by a folder node with
// the title 1 having the single child of type url with name b, followed by
// the url node with the title c.
//
// NOTE: each name must be unique, and folders are assigned a unique title by
// way of an increasing integer.
void PopulateNodeFromString(const std::string& description, TestNode* parent) {
  std::vector<std::string> elements = base::SplitString(
      description, base::kWhitespaceASCII,
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  size_t index = 0;
  PopulateNodeImpl(elements, &index, parent);
}

// Populates the BookmarkNode with the children of parent.
void PopulateBookmarkNode(TestNode* parent,
                          BookmarkModel* model,
                          const BookmarkNode* bb_node) {
  for (size_t i = 0; i < parent->children().size(); ++i) {
    TestNode* child = parent->children()[i].get();
    if (child->value == BookmarkNode::FOLDER) {
      const BookmarkNode* new_bb_node =
          model->AddFolder(bb_node, i, child->GetTitle());
      PopulateBookmarkNode(child, model, new_bb_node);
    } else {
      model->AddURL(bb_node, i, child->GetTitle(),
                    GURL("http://" + base::UTF16ToASCII(child->GetTitle())));
    }
  }
}

// Verifies the contents of the bookmark bar node match the contents of the
// TestNode.
void VerifyModelMatchesNode(TestNode* expected, const BookmarkNode* actual) {
  ASSERT_EQ(expected->children().size(), actual->children().size());
  for (size_t i = 0; i < expected->children().size(); ++i) {
    TestNode* expected_child = expected->children()[i].get();
    const BookmarkNode* actual_child = actual->children()[i].get();
    ASSERT_EQ(expected_child->GetTitle(), actual_child->GetTitle());
    if (expected_child->value == BookmarkNode::FOLDER) {
      ASSERT_TRUE(actual_child->type() == BookmarkNode::FOLDER);
      // Recurse throught children.
      VerifyModelMatchesNode(expected_child, actual_child);
    } else {
      // No need to check the URL, just the title is enough.
      ASSERT_TRUE(actual_child->is_url());
    }
  }
}

void VerifyNoDuplicateIDs(BookmarkModel* model) {
  ui::TreeNodeIterator<const BookmarkNode> it(model->root_node());
  std::unordered_set<int64_t> ids;
  while (it.has_next())
    ASSERT_TRUE(ids.insert(it.Next()->id()).second);
}

class BookmarkModelTest : public testing::Test,
                          public BookmarkModelObserver,
                          public BookmarkUndoDelegate {
 public:
  struct ObserverDetails {
    ObserverDetails() { Set(nullptr, nullptr, size_t{-1}, size_t{-1}); }

    void Set(const BookmarkNode* node1,
             const BookmarkNode* node2,
             size_t index1,
             size_t index2) {
      node1_ = node1;
      node2_ = node2;
      index1_ = index1;
      index2_ = index2;
    }

    void ExpectEquals(const BookmarkNode* node1,
                      const BookmarkNode* node2,
                      size_t index1,
                      size_t index2) {
      EXPECT_EQ(node1_, node1);
      EXPECT_EQ(node2_, node2);
      EXPECT_EQ(index1_, index1);
      EXPECT_EQ(index2_, index2);
    }

   private:
    const BookmarkNode* node1_;
    const BookmarkNode* node2_;
    size_t index1_;
    size_t index2_;
  };

  struct NodeRemovalDetail {
    NodeRemovalDetail(const BookmarkNode* parent,
                      size_t index,
                      const BookmarkNode* node)
        : parent_node_id(parent->id()), index(index), node_id(node->id()) {}

    bool operator==(const NodeRemovalDetail& other) const {
      return parent_node_id == other.parent_node_id &&
             index == other.index &&
             node_id == other.node_id;
    }

    int64_t parent_node_id;
    size_t index;
    int64_t node_id;
  };

  BookmarkModelTest() : model_(TestBookmarkClient::CreateModel()) {
    model_->AddObserver(this);
    ClearCounts();
  }

  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override {
    // We never load from the db, so that this should never get invoked.
    NOTREACHED();
  }

  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {
    ++moved_count_;
    observer_details_.Set(old_parent, new_parent, old_index, new_index);
  }

  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         size_t index) override {
    ++added_count_;
    observer_details_.Set(parent, nullptr, index, size_t{-1});
  }

  void OnWillRemoveBookmarks(BookmarkModel* model,
                             const BookmarkNode* parent,
                             size_t old_index,
                             const BookmarkNode* node) override {
    ++before_remove_count_;
  }

  void SetUndoProvider(BookmarkUndoProvider* provider) override {}

  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override {
    ++removed_count_;
    observer_details_.Set(parent, nullptr, old_index, size_t{-1});
  }

  void BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) override {
    ++changed_count_;
    observer_details_.Set(node, nullptr, size_t{-1}, size_t{-1});
  }

  void OnWillChangeBookmarkNode(BookmarkModel* model,
                                const BookmarkNode* node) override {
    ++before_change_count_;
  }

  void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                     const BookmarkNode* node) override {
    ++reordered_count_;
  }

  void OnWillReorderBookmarkNode(BookmarkModel* model,
                                 const BookmarkNode* node) override {
    ++before_reorder_count_;
  }

  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override {
    // We never attempt to load favicons, so that this method never
    // gets invoked.
  }

  void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) override {
    ++extensive_changes_beginning_count_;
  }

  void ExtensiveBookmarkChangesEnded(BookmarkModel* model) override {
    ++extensive_changes_ended_count_;
  }

  void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {
    ++all_bookmarks_removed_;
  }

  void OnWillRemoveAllUserBookmarks(BookmarkModel* model) override {
    ++before_remove_all_count_;
  }

  void GroupedBookmarkChangesBeginning(BookmarkModel* model) override {
    ++grouped_changes_beginning_count_;
  }

  void GroupedBookmarkChangesEnded(BookmarkModel* model) override {
    ++grouped_changes_ended_count_;
  }

  void OnBookmarkNodeRemoved(BookmarkModel* model,
                             const BookmarkNode* parent,
                             size_t index,
                             std::unique_ptr<BookmarkNode> node) override {
    node_removal_details_.push_back(
        NodeRemovalDetail(parent, index, node.get()));
  }

  void ClearCounts() {
    added_count_ = moved_count_ = removed_count_ = changed_count_ =
        reordered_count_ = extensive_changes_beginning_count_ =
        extensive_changes_ended_count_ = all_bookmarks_removed_ =
        before_remove_count_ = before_change_count_ = before_reorder_count_ =
        before_remove_all_count_ = grouped_changes_beginning_count_ =
        grouped_changes_ended_count_ = 0;
  }

  void AssertObserverCount(int added_count,
                           int moved_count,
                           int removed_count,
                           int changed_count,
                           int reordered_count,
                           int before_remove_count,
                           int before_change_count,
                           int before_reorder_count,
                           int before_remove_all_count) {
    EXPECT_EQ(added_count, added_count_);
    EXPECT_EQ(moved_count, moved_count_);
    EXPECT_EQ(removed_count, removed_count_);
    EXPECT_EQ(changed_count, changed_count_);
    EXPECT_EQ(reordered_count, reordered_count_);
    EXPECT_EQ(before_remove_count, before_remove_count_);
    EXPECT_EQ(before_change_count, before_change_count_);
    EXPECT_EQ(before_reorder_count, before_reorder_count_);
    EXPECT_EQ(before_remove_all_count, before_remove_all_count_);
  }

  void AssertExtensiveChangesObserverCount(
      int extensive_changes_beginning_count,
      int extensive_changes_ended_count) {
    EXPECT_EQ(extensive_changes_beginning_count,
              extensive_changes_beginning_count_);
    EXPECT_EQ(extensive_changes_ended_count, extensive_changes_ended_count_);
  }

  void AssertGroupedChangesObserverCount(
      int grouped_changes_beginning_count,
      int grouped_changes_ended_count) {
    EXPECT_EQ(grouped_changes_beginning_count,
              grouped_changes_beginning_count_);
    EXPECT_EQ(grouped_changes_ended_count, grouped_changes_ended_count_);
  }

  int AllNodesRemovedObserverCount() const { return all_bookmarks_removed_; }

  BookmarkPermanentNode* ReloadModelWithManagedNode() {
    model_->RemoveObserver(this);

    auto owned_managed_node =
        std::make_unique<BookmarkPermanentNode>(100, BookmarkNode::FOLDER);
    BookmarkPermanentNode* managed_node = owned_managed_node.get();

    std::unique_ptr<TestBookmarkClient> client(new TestBookmarkClient);
    client->SetManagedNodeToLoad(std::move(owned_managed_node));

    model_ = TestBookmarkClient::CreateModelWithClient(std::move(client));
    model_->AddObserver(this);
    ClearCounts();

    if (model_->root_node()->GetIndexOf(managed_node) == -1)
      ADD_FAILURE();

    return managed_node;
  }

 protected:
  std::unique_ptr<BookmarkModel> model_;
  ObserverDetails observer_details_;
  std::vector<NodeRemovalDetail> node_removal_details_;

 private:
  int added_count_;
  int moved_count_;
  int removed_count_;
  int changed_count_;
  int reordered_count_;
  int extensive_changes_beginning_count_;
  int extensive_changes_ended_count_;
  int all_bookmarks_removed_;
  int before_remove_count_;
  int before_change_count_;
  int before_reorder_count_;
  int before_remove_all_count_;
  int grouped_changes_beginning_count_;
  int grouped_changes_ended_count_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTest);
};

TEST_F(BookmarkModelTest, InitialState) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  ASSERT_TRUE(bb_node != nullptr);
  EXPECT_EQ(0u, bb_node->children().size());
  EXPECT_EQ(BookmarkNode::BOOKMARK_BAR, bb_node->type());

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_TRUE(other_node != nullptr);
  EXPECT_EQ(0u, other_node->children().size());
  EXPECT_EQ(BookmarkNode::OTHER_NODE, other_node->type());

  const BookmarkNode* mobile_node = model_->mobile_node();
  ASSERT_TRUE(mobile_node != nullptr);
  EXPECT_EQ(0u, mobile_node->children().size());
  EXPECT_EQ(BookmarkNode::MOBILE, mobile_node->type());

  EXPECT_TRUE(bb_node->id() != other_node->id());
  EXPECT_TRUE(bb_node->id() != mobile_node->id());
  EXPECT_TRUE(other_node->id() != mobile_node->id());
}

TEST_F(BookmarkModelTest, AddURL) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  const BookmarkNode* new_node = model_->AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  ASSERT_EQ(1u, root->children().size());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->url());
  ASSERT_TRUE(!new_node->guid().empty());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_TRUE(new_node == model_->GetMostRecentlyAddedUserNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_->other_node()->id() &&
              new_node->id() != model_->mobile_node()->id());
}

TEST_F(BookmarkModelTest, AddURLWithUnicodeTitle) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(base::WideToUTF16(
      L"\u767e\u5ea6\u4e00\u4e0b\uff0c\u4f60\u5c31\u77e5\u9053"));
  const GURL url("https://www.baidu.com/");

  const BookmarkNode* new_node = model_->AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  ASSERT_EQ(1u, root->children().size());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_TRUE(new_node == model_->GetMostRecentlyAddedUserNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_->other_node()->id() &&
              new_node->id() != model_->mobile_node()->id());
}

TEST_F(BookmarkModelTest, AddURLWithWhitespaceTitle) {
  for (size_t i = 0; i < base::size(url_whitespace_test_cases); ++i) {
    const BookmarkNode* root = model_->bookmark_bar_node();
    const base::string16 title(
        ASCIIToUTF16(url_whitespace_test_cases[i].input_title));
    const GURL url("http://foo.com");

    const BookmarkNode* new_node = model_->AddURL(root, i, title, url);

    EXPECT_EQ(i + 1, root->children().size());
    EXPECT_EQ(ASCIIToUTF16(url_whitespace_test_cases[i].expected_title),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::URL, new_node->type());
  }
}

TEST_F(BookmarkModelTest, AddURLWithCreationTimeAndMetaInfo) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const Time time = Time::Now() - TimeDelta::FromDays(1);
  BookmarkNode::MetaInfoMap meta_info;
  meta_info["foo"] = "bar";

  const BookmarkNode* new_node =
      model_->AddURL(root, 0, title, url, &meta_info, time);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  ASSERT_EQ(1u, root->children().size());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->url());
  ASSERT_TRUE(!new_node->guid().empty());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(time, new_node->date_added());
  ASSERT_TRUE(new_node->GetMetaInfoMap());
  ASSERT_EQ(meta_info, *new_node->GetMetaInfoMap());
  ASSERT_TRUE(new_node == model_->GetMostRecentlyAddedUserNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_->other_node()->id() &&
              new_node->id() != model_->mobile_node()->id());
}

TEST_F(BookmarkModelTest, AddURLWithGUID) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const Time time = Time::Now() - TimeDelta::FromDays(1);
  BookmarkNode::MetaInfoMap meta_info;
  const std::string guid = base::GenerateGUID();

  const BookmarkNode* new_node =
      model_->AddURL(root, /*index=*/0, title, url, &meta_info, time, guid);

  EXPECT_EQ(guid, new_node->guid());
}

TEST_F(BookmarkModelTest, AddURLToMobileBookmarks) {
  const BookmarkNode* root = model_->mobile_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  const BookmarkNode* new_node = model_->AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  ASSERT_EQ(1u, root->children().size());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_TRUE(new_node == model_->GetMostRecentlyAddedUserNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_->other_node()->id() &&
              new_node->id() != model_->mobile_node()->id());
}

TEST_F(BookmarkModelTest, AddFolder) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));

  const BookmarkNode* new_node = model_->AddFolder(root, 0, title);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  ASSERT_EQ(1u, root->children().size());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(!new_node->guid().empty());
  ASSERT_EQ(BookmarkNode::FOLDER, new_node->type());

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_->other_node()->id() &&
              new_node->id() != model_->mobile_node()->id());

  // Add another folder, just to make sure folder_ids are incremented correctly.
  ClearCounts();
  model_->AddFolder(root, 0, title);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});
}

TEST_F(BookmarkModelTest, AddFolderWithGUID) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  BookmarkNode::MetaInfoMap meta_info;
  const std::string guid = base::GenerateGUID();

  const BookmarkNode* new_node =
      model_->AddFolder(root, /*index=*/0, title, &meta_info, guid);

  EXPECT_EQ(guid, new_node->guid());
}

TEST_F(BookmarkModelTest, AddFolderWithWhitespaceTitle) {
  for (size_t i = 0; i < base::size(title_whitespace_test_cases); ++i) {
    const BookmarkNode* root = model_->bookmark_bar_node();
    const base::string16 title(
        ASCIIToUTF16(title_whitespace_test_cases[i].input_title));

    const BookmarkNode* new_node = model_->AddFolder(root, i, title);

    EXPECT_EQ(i + 1, root->children().size());
    EXPECT_EQ(ASCIIToUTF16(title_whitespace_test_cases[i].expected_title),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  }
}

TEST_F(BookmarkModelTest, RemoveURL) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  model_->AddURL(root, 0, title, url);
  ClearCounts();

  model_->Remove(root->children().front().get());
  ASSERT_EQ(0u, root->children().size());
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model_->GetMostRecentlyAddedUserNodeForURL(url) == nullptr);
}

TEST_F(BookmarkModelTest, RemoveFolder) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const BookmarkNode* folder = model_->AddFolder(root, 0, ASCIIToUTF16("foo"));

  ClearCounts();

  // Add a URL as a child.
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  model_->AddURL(folder, 0, title, url);

  ClearCounts();

  // Now remove the folder.
  model_->Remove(root->children().front().get());
  ASSERT_EQ(0u, root->children().size());
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model_->GetMostRecentlyAddedUserNodeForURL(url) == nullptr);
}

TEST_F(BookmarkModelTest, RemoveAllUserBookmarks) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  ClearCounts();

  // Add a url to bookmark bar.
  base::string16 title(ASCIIToUTF16("foo"));
  GURL url("http://foo.com");
  const BookmarkNode* url_node =
      model_->AddURL(bookmark_bar_node, 0, title, url);

  // Add a folder with child URL.
  const BookmarkNode* folder = model_->AddFolder(bookmark_bar_node, 0, title);
  model_->AddURL(folder, 0, title, url);

  AssertObserverCount(3, 0, 0, 0, 0, 0, 0, 0, 0);
  ClearCounts();

  size_t permanent_node_count = model_->root_node()->children().size();

  NodeRemovalDetail expected_node_removal_details[] = {
    NodeRemovalDetail(bookmark_bar_node, 1, url_node),
    NodeRemovalDetail(bookmark_bar_node, 0, folder),
  };

  model_->SetUndoDelegate(this);
  model_->RemoveAllUserBookmarks();

  EXPECT_EQ(0u, bookmark_bar_node->children().size());
  // No permanent node should be removed.
  EXPECT_EQ(permanent_node_count, model_->root_node()->children().size());
  // No individual BookmarkNodeRemoved events are fired, so removed count
  // should be 0.
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 1);
  AssertExtensiveChangesObserverCount(1, 1);
  AssertGroupedChangesObserverCount(1, 1);
  EXPECT_EQ(1, AllNodesRemovedObserverCount());
  EXPECT_EQ(1, AllNodesRemovedObserverCount());
  ASSERT_EQ(2u, node_removal_details_.size());
  EXPECT_EQ(expected_node_removal_details[0], node_removal_details_[0]);
  EXPECT_EQ(expected_node_removal_details[1], node_removal_details_[1]);
}

TEST_F(BookmarkModelTest, SetTitle) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const BookmarkNode* node = model_->AddURL(root, 0, title, url);

  ClearCounts();

  title = ASCIIToUTF16("foo2");
  model_->SetTitle(node, title);
  AssertObserverCount(0, 0, 0, 1, 0, 0, 1, 0, 0);
  observer_details_.ExpectEquals(node, nullptr, size_t{-1}, size_t{-1});
  EXPECT_EQ(title, node->GetTitle());
}

TEST_F(BookmarkModelTest, SetTitleWithWhitespace) {
  for (size_t i = 0; i < base::size(title_whitespace_test_cases); ++i) {
    const BookmarkNode* root = model_->bookmark_bar_node();
    base::string16 title(ASCIIToUTF16("dummy"));
    const GURL url("http://foo.com");
    const BookmarkNode* node = model_->AddURL(root, 0, title, url);

    title = ASCIIToUTF16(title_whitespace_test_cases[i].input_title);
    model_->SetTitle(node, title);
    EXPECT_EQ(ASCIIToUTF16(title_whitespace_test_cases[i].expected_title),
              node->GetTitle());
  }
}

TEST_F(BookmarkModelTest, SetURL) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  GURL url("http://foo.com");
  const BookmarkNode* node = model_->AddURL(root, 0, title, url);

  ClearCounts();

  url = GURL("http://foo2.com");
  model_->SetURL(node, url);
  AssertObserverCount(0, 0, 0, 1, 0, 0, 1, 0, 0);
  observer_details_.ExpectEquals(node, nullptr, size_t{-1}, size_t{-1});
  EXPECT_EQ(url, node->url());
}

TEST_F(BookmarkModelTest, SetDateAdded) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  GURL url("http://foo.com");
  const BookmarkNode* node = model_->AddURL(root, 0, title, url);

  ClearCounts();

  base::Time new_time = base::Time::Now() + base::TimeDelta::FromMinutes(20);
  model_->SetDateAdded(node, new_time);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(new_time, node->date_added());
  EXPECT_EQ(new_time, model_->bookmark_bar_node()->date_folder_modified());
}

TEST_F(BookmarkModelTest, Move) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const BookmarkNode* node = model_->AddURL(root, 0, title, url);
  const BookmarkNode* folder1 = model_->AddFolder(root, 0, ASCIIToUTF16("foo"));
  ClearCounts();

  model_->Move(node, folder1, 0);

  AssertObserverCount(0, 1, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, folder1, 1, 0);
  EXPECT_TRUE(folder1 == node->parent());
  EXPECT_EQ(1u, root->children().size());
  EXPECT_EQ(folder1, root->children().front().get());
  EXPECT_EQ(1u, folder1->children().size());
  EXPECT_EQ(node, folder1->children().front().get());

  // And remove the folder.
  ClearCounts();
  model_->Remove(root->children().front().get());
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0);
  observer_details_.ExpectEquals(root, nullptr, 0, size_t{-1});
  EXPECT_TRUE(model_->GetMostRecentlyAddedUserNodeForURL(url) == nullptr);
  EXPECT_EQ(0u, root->children().size());
}

TEST_F(BookmarkModelTest, NonMovingMoveCall) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const base::Time old_date(base::Time::Now() - base::TimeDelta::FromDays(1));

  const BookmarkNode* node = model_->AddURL(root, 0, title, url);
  model_->SetDateFolderModified(root, old_date);

  // Since |node| is already at the index 0 of |root|, this is no-op.
  model_->Move(node, root, 0);

  // Check that the modification date is kept untouched.
  EXPECT_EQ(old_date, root->date_folder_modified());
}

TEST_F(BookmarkModelTest, Copy) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ e f g ] h ");
  test::AddNodesFromModelString(model_.get(), root, model_string);

  // Validate initial model.
  std::string actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actual_model_string);

  // Copy 'd' to be after '1:b': URL item from bar to folder.
  const BookmarkNode* node_to_copy = root->children()[2].get();
  const BookmarkNode* destination = root->children()[1].get();
  model_->Copy(node_to_copy, destination, 1);
  actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ("a 1:[ b d c ] d 2:[ e f g ] h ", actual_model_string);

  // Copy '1:d' to be after 'a': URL item from folder to bar.
  const BookmarkNode* folder = root->children()[1].get();
  node_to_copy = folder->children()[1].get();
  model_->Copy(node_to_copy, root, 1);
  actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e f g ] h ", actual_model_string);

  // Copy '1' to be after '2:e': Folder from bar to folder.
  node_to_copy = root->children()[2].get();
  destination = root->children()[4].get();
  model_->Copy(node_to_copy, destination, 1);
  actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e 1:[ b d c ] f g ] h ",
            actual_model_string);

  // Copy '2:1' to be after '2:f': Folder within same folder.
  folder = root->children()[4].get();
  node_to_copy = folder->children()[1].get();
  model_->Copy(node_to_copy, folder, 3);
  actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] h ",
            actual_model_string);

  // Copy first 'd' to be after 'h': URL item within the bar.
  node_to_copy = root->children()[1].get();
  model_->Copy(node_to_copy, root, 6);
  actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] h d ",
            actual_model_string);

  // Copy '2' to be after 'a': Folder within the bar.
  node_to_copy = root->children()[4].get();
  model_->Copy(node_to_copy, root, 1);
  actual_model_string = test::ModelStringFromNode(root);
  EXPECT_EQ("a 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] d 1:[ b d c ] "
            "d 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] h d ",
            actual_model_string);
}

// Tests the default node if no bookmarks have been added yet
TEST_F(BookmarkModelTest, ParentForNewNodesWithEmptyModel) {
#if defined(OS_ANDROID)
  ASSERT_EQ(model_->mobile_node(), GetParentForNewNodes(model_.get()));
#else
  ASSERT_EQ(model_->bookmark_bar_node(), GetParentForNewNodes(model_.get()));
#endif
}

#if defined(OS_ANDROID)
// Tests that the bookmark_bar_node can still be returned even on Android in
// case the last bookmark was added to it.
TEST_F(BookmarkModelTest, ParentCanBeBookmarkBarOnAndroid) {
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_->AddURL(model_->bookmark_bar_node(), 0, title, url);
  ASSERT_EQ(model_->bookmark_bar_node(), GetParentForNewNodes(model_.get()));
}
#endif

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewNodes) {
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_->AddURL(model_->other_node(), 0, title, url);
  ASSERT_EQ(model_->other_node(), GetParentForNewNodes(model_.get()));
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewMobileNodes) {
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_->AddURL(model_->mobile_node(), 0, title, url);
  ASSERT_EQ(model_->mobile_node(), GetParentForNewNodes(model_.get()));
}

// Make sure recently modified stays in sync when adding a URL.
TEST_F(BookmarkModelTest, MostRecentlyModifiedFolders) {
  // Add a folder.
  const BookmarkNode* folder =
      model_->AddFolder(model_->other_node(), 0, ASCIIToUTF16("foo"));
  // Add a URL to it.
  model_->AddURL(folder, 0, ASCIIToUTF16("blah"), GURL("http://foo.com"));

  // Make sure folder is in the most recently modified.
  std::vector<const BookmarkNode*> most_recent_folders =
      GetMostRecentlyModifiedUserFolders(model_.get(), 1);
  ASSERT_EQ(1U, most_recent_folders.size());
  ASSERT_EQ(folder, most_recent_folders[0]);

  // Nuke the folder and do another fetch, making sure folder isn't in the
  // returned list.
  model_->Remove(folder->parent()->children().front().get());
  most_recent_folders = GetMostRecentlyModifiedUserFolders(model_.get(), 1);
  ASSERT_EQ(1U, most_recent_folders.size());
  ASSERT_TRUE(most_recent_folders[0] != folder);
}

// Make sure MostRecentlyAddedEntries stays in sync.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  Time base_time = Time::Now();
  BookmarkNode* n1 = AsMutable(model_->AddURL(model_->bookmark_bar_node(),
                                              0,
                                              ASCIIToUTF16("blah"),
                                              GURL("http://foo.com/0")));
  BookmarkNode* n2 = AsMutable(model_->AddURL(model_->bookmark_bar_node(),
                                              1,
                                              ASCIIToUTF16("blah"),
                                              GURL("http://foo.com/1")));
  BookmarkNode* n3 = AsMutable(model_->AddURL(model_->bookmark_bar_node(),
                                              2,
                                              ASCIIToUTF16("blah"),
                                              GURL("http://foo.com/2")));
  BookmarkNode* n4 = AsMutable(model_->AddURL(model_->bookmark_bar_node(),
                                              3,
                                              ASCIIToUTF16("blah"),
                                              GURL("http://foo.com/3")));
  n1->set_date_added(base_time + TimeDelta::FromDays(4));
  n2->set_date_added(base_time + TimeDelta::FromDays(3));
  n3->set_date_added(base_time + TimeDelta::FromDays(2));
  n4->set_date_added(base_time + TimeDelta::FromDays(1));

  // Make sure order is honored.
  std::vector<const BookmarkNode*> recently_added;
  GetMostRecentlyAddedEntries(model_.get(), 2, &recently_added);
  ASSERT_EQ(2U, recently_added.size());
  ASSERT_TRUE(n1 == recently_added[0]);
  ASSERT_TRUE(n2 == recently_added[1]);

  // swap 1 and 2, then check again.
  recently_added.clear();
  SwapDateAdded(n1, n2);
  GetMostRecentlyAddedEntries(model_.get(), 4, &recently_added);
  ASSERT_EQ(4U, recently_added.size());
  ASSERT_TRUE(n2 == recently_added[0]);
  ASSERT_TRUE(n1 == recently_added[1]);
  ASSERT_TRUE(n3 == recently_added[2]);
  ASSERT_TRUE(n4 == recently_added[3]);
}

// Makes sure GetMostRecentlyAddedUserNodeForURL stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedUserNodeForURL) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2
  Time base_time = Time::Now();
  const GURL url("http://foo.com/0");
  BookmarkNode* n1 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 0, ASCIIToUTF16("blah"), url));
  BookmarkNode* n2 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 1, ASCIIToUTF16("blah"), url));
  n1->set_date_added(base_time + TimeDelta::FromDays(4));
  n2->set_date_added(base_time + TimeDelta::FromDays(3));

  // Make sure order is honored.
  ASSERT_EQ(n1, model_->GetMostRecentlyAddedUserNodeForURL(url));

  // swap 1 and 2, then check again.
  SwapDateAdded(n1, n2);
  ASSERT_EQ(n2, model_->GetMostRecentlyAddedUserNodeForURL(url));
}

// Makes sure GetBookmarks removes duplicates.
TEST_F(BookmarkModelTest, GetBookmarksWithDups) {
  const GURL url("http://foo.com/0");
  const base::string16 title(ASCIIToUTF16("blah"));
  model_->AddURL(model_->bookmark_bar_node(), 0, title, url);
  model_->AddURL(model_->bookmark_bar_node(), 1, title, url);

  std::vector<UrlAndTitle> bookmarks;
  model_->GetBookmarks(&bookmarks);
  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_EQ(url, bookmarks[0].url);
  EXPECT_EQ(title, bookmarks[0].title);

  model_->AddURL(model_->bookmark_bar_node(), 2, ASCIIToUTF16("Title2"), url);
  // Only one returned, even titles are different.
  bookmarks.clear();
  model_->GetBookmarks(&bookmarks);
  EXPECT_EQ(1U, bookmarks.size());
}

TEST_F(BookmarkModelTest, HasBookmarks) {
  const GURL url("http://foo.com/");
  model_->AddURL(model_->bookmark_bar_node(), 0, ASCIIToUTF16("bar"), url);

  EXPECT_TRUE(model_->HasBookmarks());
}

// http://crbug.com/450464
TEST_F(BookmarkModelTest, DISABLED_Sort) {
  // Populate the bookmark bar node with nodes for 'B', 'a', 'd' and 'C'.
  // 'C' and 'a' are folders.
  TestNode bbn;
  PopulateNodeFromString("B [ a ] d [ a ]", &bbn);
  const BookmarkNode* parent = model_->bookmark_bar_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);

  BookmarkNode* child1 = parent->children()[1].get();
  child1->SetTitle(ASCIIToUTF16("a"));
  child1->Remove(0);
  BookmarkNode* child3 = parent->children()[3].get();
  child3->SetTitle(ASCIIToUTF16("C"));
  child3->Remove(0);

  ClearCounts();

  // Sort the children of the bookmark bar node.
  model_->SortChildren(parent);

  // Make sure we were notified.
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 1, 0);

  // Make sure the order matches (remember, 'a' and 'C' are folders and
  // come first).
  EXPECT_EQ(parent->children()[0]->GetTitle(), ASCIIToUTF16("a"));
  EXPECT_EQ(parent->children()[1]->GetTitle(), ASCIIToUTF16("C"));
  EXPECT_EQ(parent->children()[2]->GetTitle(), ASCIIToUTF16("B"));
  EXPECT_EQ(parent->children()[3]->GetTitle(), ASCIIToUTF16("d"));
}

TEST_F(BookmarkModelTest, Reorder) {
  // Populate the bookmark bar node with nodes 'A', 'B', 'C' and 'D'.
  TestNode bbn;
  PopulateNodeFromString("A B C D", &bbn);
  BookmarkNode* parent = AsMutable(model_->bookmark_bar_node());
  PopulateBookmarkNode(&bbn, model_.get(), parent);

  ClearCounts();

  // Reorder bar node's bookmarks in reverse order.
  std::vector<const BookmarkNode*> new_order = {
      parent->children()[3].get(),
      parent->children()[2].get(),
      parent->children()[1].get(),
      parent->children()[0].get(),
  };
  model_->ReorderChildren(parent, new_order);

  // Make sure we were notified.
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 1, 0);

  // Make sure the order matches is correct (it should be reversed).
  ASSERT_EQ(4u, parent->children().size());
  EXPECT_EQ("D", base::UTF16ToASCII(parent->children()[0]->GetTitle()));
  EXPECT_EQ("C", base::UTF16ToASCII(parent->children()[1]->GetTitle()));
  EXPECT_EQ("B", base::UTF16ToASCII(parent->children()[2]->GetTitle()));
  EXPECT_EQ("A", base::UTF16ToASCII(parent->children()[3]->GetTitle()));
}

TEST_F(BookmarkModelTest, NodeVisibility) {
  // Mobile node invisible by default
  EXPECT_TRUE(model_->bookmark_bar_node()->IsVisible());
  EXPECT_TRUE(model_->other_node()->IsVisible());
  EXPECT_FALSE(model_->mobile_node()->IsVisible());

  // Visibility of permanent node can only be changed if they are not
  // forced to be visible by the client.
  model_->SetPermanentNodeVisible(BookmarkNode::BOOKMARK_BAR, false);
  EXPECT_TRUE(model_->bookmark_bar_node()->IsVisible());
  model_->SetPermanentNodeVisible(BookmarkNode::OTHER_NODE, false);
  EXPECT_TRUE(model_->other_node()->IsVisible());
  model_->SetPermanentNodeVisible(BookmarkNode::MOBILE, true);
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
  model_->SetPermanentNodeVisible(BookmarkNode::MOBILE, false);
  EXPECT_FALSE(model_->mobile_node()->IsVisible());

  // Arbitrary node should be visible
  TestNode bbn;
  PopulateNodeFromString("B", &bbn);
  const BookmarkNode* parent = model_->mobile_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  // Mobile folder should be visible now that it has a child.
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
}

TEST_F(BookmarkModelTest, MobileNodeVisibleWithChildren) {
  const BookmarkNode* root = model_->mobile_node();
  const base::string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_->AddURL(root, 0, title, url);
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
}

TEST_F(BookmarkModelTest, ExtensiveChangesObserver) {
  AssertExtensiveChangesObserverCount(0, 0);
  EXPECT_FALSE(model_->IsDoingExtensiveChanges());
  model_->BeginExtensiveChanges();
  EXPECT_TRUE(model_->IsDoingExtensiveChanges());
  AssertExtensiveChangesObserverCount(1, 0);
  model_->EndExtensiveChanges();
  EXPECT_FALSE(model_->IsDoingExtensiveChanges());
  AssertExtensiveChangesObserverCount(1, 1);
}

TEST_F(BookmarkModelTest, MultipleExtensiveChangesObserver) {
  AssertExtensiveChangesObserverCount(0, 0);
  EXPECT_FALSE(model_->IsDoingExtensiveChanges());
  model_->BeginExtensiveChanges();
  EXPECT_TRUE(model_->IsDoingExtensiveChanges());
  AssertExtensiveChangesObserverCount(1, 0);
  model_->BeginExtensiveChanges();
  EXPECT_TRUE(model_->IsDoingExtensiveChanges());
  AssertExtensiveChangesObserverCount(1, 0);
  model_->EndExtensiveChanges();
  EXPECT_TRUE(model_->IsDoingExtensiveChanges());
  AssertExtensiveChangesObserverCount(1, 0);
  model_->EndExtensiveChanges();
  EXPECT_FALSE(model_->IsDoingExtensiveChanges());
  AssertExtensiveChangesObserverCount(1, 1);
}

// Verifies that IsBookmarked is true if any bookmark matches the given URL,
// and that IsBookmarkedByUser is true only if at least one of the matching
// bookmarks can be edited by the user.
TEST_F(BookmarkModelTest, IsBookmarked) {
  // Reload the model with a managed node that is not editable by the user.
  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();

  // "google.com" is a "user" bookmark.
  model_->AddURL(model_->other_node(), 0, base::ASCIIToUTF16("User"),
                 GURL("http://google.com"));
  // "youtube.com" is not.
  model_->AddURL(managed_node, 0, base::ASCIIToUTF16("Managed"),
                 GURL("http://youtube.com"));

  EXPECT_TRUE(model_->IsBookmarked(GURL("http://google.com")));
  EXPECT_TRUE(model_->IsBookmarked(GURL("http://youtube.com")));
  EXPECT_FALSE(model_->IsBookmarked(GURL("http://reddit.com")));

  EXPECT_TRUE(IsBookmarkedByUser(model_.get(), GURL("http://google.com")));
  EXPECT_FALSE(IsBookmarkedByUser(model_.get(), GURL("http://youtube.com")));
  EXPECT_FALSE(IsBookmarkedByUser(model_.get(), GURL("http://reddit.com")));
}

// Verifies that GetMostRecentlyAddedUserNodeForURL skips bookmarks that
// are not owned by the user.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedUserNodeForURLSkipsManagedNodes) {
  // Reload the model with a managed node that is not editable by the user.
  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();

  const base::string16 title = base::ASCIIToUTF16("Title");
  const BookmarkNode* user_parent = model_->other_node();
  const BookmarkNode* managed_parent = managed_node;
  const GURL url("http://google.com");

  // |url| is not bookmarked yet.
  EXPECT_TRUE(model_->GetMostRecentlyAddedUserNodeForURL(url) == nullptr);

  // Having a managed node doesn't count.
  model_->AddURL(managed_parent, 0, title, url);
  EXPECT_TRUE(model_->GetMostRecentlyAddedUserNodeForURL(url) == nullptr);

  // Now add a user node.
  const BookmarkNode* user = model_->AddURL(user_parent, 0, title, url);
  EXPECT_EQ(user, model_->GetMostRecentlyAddedUserNodeForURL(url));

  // Having a more recent managed node doesn't count either.
  const BookmarkNode* managed = model_->AddURL(managed_parent, 0, title, url);
  EXPECT_GE(managed->date_added(), user->date_added());
  EXPECT_EQ(user, model_->GetMostRecentlyAddedUserNodeForURL(url));
}

// Verifies that renaming a bookmark folder does not add the folder node to the
// autocomplete index. crbug.com/778266
TEST_F(BookmarkModelTest, RenamedFolderNodeExcludedFromIndex) {
  // Add a folder.
  const BookmarkNode* folder =
      model_->AddFolder(model_->other_node(), 0, ASCIIToUTF16("MyFavorites"));

  // Change the folder title.
  model_->SetTitle(folder, ASCIIToUTF16("MyBookmarks"));

  // There should be no matching bookmarks.
  std::vector<TitledUrlMatch> matches;
  model_->GetBookmarksMatching(ASCIIToUTF16("MyB"), /*max_count = */ 1,
                               query_parser::MatchingAlgorithm::DEFAULT,
                               &matches);
  EXPECT_TRUE(matches.empty());
}

// Verifies the TitledUrlIndex is probably loaded.
TEST(BookmarkModelLoadTest, TitledUrlIndexPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<BookmarkModel> model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(nullptr, tmp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get(),
              base::ThreadTaskRunnerHandle::Get());
  test::WaitForBookmarkModelToLoad(model.get());
  const GURL node_url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, base::ASCIIToUTF16("User"),
                node_url);
  // This is necessary to ensure the save completes.
  base::RunLoop().RunUntilIdle();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(nullptr, tmp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get(),
              base::ThreadTaskRunnerHandle::Get());
  test::WaitForBookmarkModelToLoad(model.get());

  std::vector<TitledUrlMatch> matches;
  model->GetBookmarksMatching(base::ASCIIToUTF16("user"), 1,
                              query_parser::MatchingAlgorithm::DEFAULT,
                              &matches);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(node_url, matches[0].node->GetTitledUrlNodeUrl());
}

TEST(BookmarkNodeTest, NodeMetaInfo) {
  GURL url;
  BookmarkNode node(/*id=*/0, base::GenerateGUID(), url);
  EXPECT_FALSE(node.GetMetaInfoMap());

  EXPECT_TRUE(node.SetMetaInfo("key1", "value1"));
  std::string out_value;
  EXPECT_TRUE(node.GetMetaInfo("key1", &out_value));
  EXPECT_EQ("value1", out_value);
  EXPECT_FALSE(node.SetMetaInfo("key1", "value1"));

  EXPECT_FALSE(node.GetMetaInfo("key2.subkey1", &out_value));
  EXPECT_TRUE(node.SetMetaInfo("key2.subkey1", "value2"));
  EXPECT_TRUE(node.GetMetaInfo("key2.subkey1", &out_value));
  EXPECT_EQ("value2", out_value);

  EXPECT_FALSE(node.GetMetaInfo("key2.subkey2.leaf", &out_value));
  EXPECT_TRUE(node.SetMetaInfo("key2.subkey2.leaf", ""));
  EXPECT_TRUE(node.GetMetaInfo("key2.subkey2.leaf", &out_value));
  EXPECT_EQ("", out_value);

  EXPECT_TRUE(node.DeleteMetaInfo("key1"));
  EXPECT_TRUE(node.DeleteMetaInfo("key2.subkey1"));
  EXPECT_TRUE(node.DeleteMetaInfo("key2.subkey2.leaf"));
  EXPECT_FALSE(node.DeleteMetaInfo("key3"));
  EXPECT_FALSE(node.GetMetaInfo("key1", &out_value));
  EXPECT_FALSE(node.GetMetaInfo("key2.subkey1", &out_value));
  EXPECT_FALSE(node.GetMetaInfo("key2.subkey2", &out_value));
  EXPECT_FALSE(node.GetMetaInfo("key2.subkey2.leaf", &out_value));
  EXPECT_FALSE(node.GetMetaInfoMap());
}

// Creates a set of nodes in the bookmark model, and checks that the loaded
// structure is what we first created.
TEST(BookmarkModelTest2, CreateAndRestore) {
  struct TestData {
    // Structure of the children of the bookmark model node.
    const std::string bbn_contents;
    // Structure of the children of the other node.
    const std::string other_contents;
    // Structure of the children of the synced node.
    const std::string mobile_contents;
  } data[] = {
    // See PopulateNodeFromString for a description of these strings.
    { "", "" },
    { "a", "b" },
    { "a [ b ]", "" },
    { "", "[ b ] a [ c [ d e [ f ] ] ]" },
    { "a [ b ]", "" },
    { "a b c [ d e [ f ] ]", "g h i [ j k [ l ] ]"},
  };
  std::unique_ptr<BookmarkModel> model;
  for (size_t i = 0; i < base::size(data); ++i) {
    model = TestBookmarkClient::CreateModel();

    TestNode bbn;
    PopulateNodeFromString(data[i].bbn_contents, &bbn);
    PopulateBookmarkNode(&bbn, model.get(), model->bookmark_bar_node());

    TestNode other;
    PopulateNodeFromString(data[i].other_contents, &other);
    PopulateBookmarkNode(&other, model.get(), model->other_node());

    TestNode mobile;
    PopulateNodeFromString(data[i].mobile_contents, &mobile);
    PopulateBookmarkNode(&mobile, model.get(), model->mobile_node());

    VerifyModelMatchesNode(&bbn, model->bookmark_bar_node());
    VerifyModelMatchesNode(&other, model->other_node());
    VerifyModelMatchesNode(&mobile, model->mobile_node());
    VerifyNoDuplicateIDs(model.get());
  }
}

}  // namespace

class BookmarkModelFaviconTest : public testing::Test,
                                 public BookmarkModelObserver {
 public:
  BookmarkModelFaviconTest() : model_(TestBookmarkClient::CreateModel()) {
    model_->AddObserver(this);
  }

  // Emulates the favicon getting asynchronously loaded. In production, the
  // favicon is asynchronously loaded when BookmarkModel::GetFavicon() is
  // called.
  void OnFaviconLoaded(BookmarkNode* node, const GURL& icon_url) {
      SkBitmap bitmap;
      bitmap.allocN32Pixels(16, 16);
      bitmap.eraseColor(SK_ColorBLUE);
      gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);

      favicon_base::FaviconImageResult image_result;
      image_result.image = image;
      image_result.icon_url = icon_url;
      model_->OnFaviconDataAvailable(node, favicon_base::IconType::kFavicon,
                                     image_result);
  }

  bool WasNodeUpdated(const BookmarkNode* node) {
    return base::Contains(updated_nodes_, node);
  }

  void ClearUpdatedNodes() {
      updated_nodes_.clear();
  }

 protected:
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override {
  }

  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {}

  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         size_t index) override {}

  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override {}

  void BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) override {}

  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override {
    updated_nodes_.push_back(node);
  }

  void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                     const BookmarkNode* node) override {}

  void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {
  }

  std::unique_ptr<BookmarkModel> model_;
  std::vector<const BookmarkNode*> updated_nodes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkModelFaviconTest);
};

// Test that BookmarkModel::OnFaviconsChanged() sends a notification that the
// favicon changed to each BookmarkNode which has either a matching page URL
// (e.g. http://www.google.com) or a matching icon URL
// (e.g. http://www.google.com/favicon.ico).
TEST_F(BookmarkModelFaviconTest, FaviconsChangedObserver) {
  const BookmarkNode* root = model_->bookmark_bar_node();
  base::string16 kTitle(ASCIIToUTF16("foo"));
  GURL kPageURL1("http://www.google.com");
  GURL kPageURL2("http://www.google.ca");
  GURL kPageURL3("http://www.amazon.com");
  GURL kFaviconURL12("http://www.google.com/favicon.ico");
  GURL kFaviconURL3("http://www.amazon.com/favicon.ico");

  const BookmarkNode* node1 = model_->AddURL(root, 0, kTitle, kPageURL1);
  const BookmarkNode* node2 = model_->AddURL(root, 0, kTitle, kPageURL2);
  const BookmarkNode* node3 = model_->AddURL(root, 0, kTitle, kPageURL3);
  const BookmarkNode* node4 = model_->AddURL(root, 0, kTitle, kPageURL3);

  {
    OnFaviconLoaded(AsMutable(node1), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node2), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node3), kFaviconURL3);
    OnFaviconLoaded(AsMutable(node4), kFaviconURL3);

    ClearUpdatedNodes();
    std::set<GURL> changed_page_urls;
    changed_page_urls.insert(kPageURL2);
    changed_page_urls.insert(kPageURL3);
    model_->OnFaviconsChanged(changed_page_urls, GURL());
    ASSERT_EQ(3u, updated_nodes_.size());
    EXPECT_TRUE(WasNodeUpdated(node2));
    EXPECT_TRUE(WasNodeUpdated(node3));
    EXPECT_TRUE(WasNodeUpdated(node4));
  }

  {
    // Reset the favicon data because BookmarkModel::OnFaviconsChanged() clears
    // the BookmarkNode's favicon data for all of the BookmarkNodes whose
    // favicon data changed.
    OnFaviconLoaded(AsMutable(node1), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node2), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node3), kFaviconURL3);
    OnFaviconLoaded(AsMutable(node4), kFaviconURL3);

    ClearUpdatedNodes();
    model_->OnFaviconsChanged(std::set<GURL>(), kFaviconURL12);
    ASSERT_EQ(2u, updated_nodes_.size());
    EXPECT_TRUE(WasNodeUpdated(node1));
    EXPECT_TRUE(WasNodeUpdated(node2));
  }

  {
    OnFaviconLoaded(AsMutable(node1), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node2), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node3), kFaviconURL3);
    OnFaviconLoaded(AsMutable(node4), kFaviconURL3);

    ClearUpdatedNodes();
    std::set<GURL> changed_page_urls;
    changed_page_urls.insert(kPageURL1);
    model_->OnFaviconsChanged(changed_page_urls, kFaviconURL12);
    ASSERT_EQ(2u, updated_nodes_.size());
    EXPECT_TRUE(WasNodeUpdated(node1));
    EXPECT_TRUE(WasNodeUpdated(node2));
  }
}

}  // namespace bookmarks
