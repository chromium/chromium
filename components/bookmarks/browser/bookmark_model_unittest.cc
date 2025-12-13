// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/query_parser/query_parser.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace bookmarks {
namespace {

using NodeTypeForUuidLookup = BookmarkModel::NodeTypeForUuidLookup;

using base::ASCIIToUTF16;
using base::Time;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::WithArg;
using testing::WithArgs;

// Test cases used to test the removal of extra whitespace when adding
// a new folder/bookmark or updating a title of a folder/bookmark.
// Note that whitespace characters are all replaced with spaces, but spaces are
// not collapsed or trimmed.
struct UrlWhitespaceTestCases {
  std::string_view input_title;
  std::string_view expected_title;
};
constexpr auto kUrlWhitespaceTestCases = std::to_array<UrlWhitespaceTestCases>({
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
});

// Test cases used to test the removal of extra whitespace when adding
// a new folder/bookmark or updating a title of a folder/bookmark.
struct TitleWhitespaceTestCases {
  std::string_view input_title;
  std::string_view expected_title;
};
constexpr auto kTitleWhitespaceTestCases =
    std::to_array<TitleWhitespaceTestCases>({
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
    });

// TestBookmarkClient that also has basic support for undoing removals.
class TestBookmarkClientWithUndo : public TestBookmarkClient {
 public:
  TestBookmarkClientWithUndo() = default;
  ~TestBookmarkClientWithUndo() override = default;

  [[nodiscard]] bool RestoreLastRemovedBookmark(BookmarkModel* model) {
    CHECK(model);

    if (!last_removed_node_) {
      return false;
    }

    static_cast<BookmarkUndoProvider*>(model)->RestoreRemovedNode(
        parent_, index_, std::move(last_removed_node_));

    parent_ = nullptr;
    index_ = 0;
    last_removed_node_ = nullptr;
    return true;
  }

  // BookmarkClient overrides.
  void OnBookmarkNodeRemovedUndoable(
      const BookmarkNode* parent,
      size_t index,
      std::unique_ptr<BookmarkNode> node) override {
    parent_ = parent;
    index_ = index;
    last_removed_node_ = std::move(node);
  }

 private:
  raw_ptr<const BookmarkNode> parent_ = nullptr;
  size_t index_ = 0;
  std::unique_ptr<BookmarkNode> last_removed_node_;
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

void SwapDateUsed(BookmarkNode* n1, BookmarkNode* n2) {
  Time tmp = n1->date_last_used();
  n1->set_date_last_used(n2->date_last_used());
  n2->set_date_last_used(tmp);
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
void PopulateNodeFromString(std::string_view description, TestNode* parent) {
  std::vector<std::string> elements =
      base::SplitString(description, base::kWhitespaceASCII,
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
      // Recurse through children.
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
  while (it.has_next()) {
    ASSERT_TRUE(ids.insert(it.Next()->id()).second);
  }
}

class BookmarkModelTest : public testing::Test, public BookmarkModelObserver {
 public:
  struct ObserverDetails {
    ObserverDetails() {
      Set(nullptr, nullptr, static_cast<size_t>(-1), static_cast<size_t>(-1),
          false, FROM_HERE);
    }

    void Set(const BookmarkNode* node1,
             const BookmarkNode* node2,
             size_t index1,
             size_t index2,
             bool added_by_user,
             const base::Location& location) {
      node1_ = node1;
      node2_ = node2;
      index1_ = index1;
      index2_ = index2;
      added_by_user_ = added_by_user;
      location_ = location;
    }

    void Reset() {
      Set(nullptr, nullptr, static_cast<size_t>(-1), static_cast<size_t>(-1),
          false, FROM_HERE);
    }

    void ExpectEquals(const BookmarkNode* node1,
                      const BookmarkNode* node2,
                      size_t index1,
                      size_t index2,
                      bool added_by_user,
                      std::optional<base::Location> location = std::nullopt) {
      EXPECT_EQ(node1_, node1);
      EXPECT_EQ(node2_, node2);
      EXPECT_EQ(index1_, index1);
      EXPECT_EQ(index2_, index2);
      EXPECT_EQ(added_by_user_, added_by_user);
      if (location.has_value()) {
        EXPECT_EQ(location_, *location);
      }
    }

   private:
    raw_ptr<const BookmarkNode> node1_;
    raw_ptr<const BookmarkNode> node2_;
    size_t index1_;
    size_t index2_;
    bool added_by_user_;
    base::Location location_;
  };

  struct AllNodesRemovedDetail {
    explicit AllNodesRemovedDetail(const std::set<GURL>& removed_urls)
        : removed_urls(removed_urls) {}

    bool operator==(const AllNodesRemovedDetail& other) const {
      return removed_urls == other.removed_urls;
    }

    std::set<GURL> removed_urls;
  };

  BookmarkModelTest()
      : model_(TestBookmarkClient::CreateModelWithClient(
            std::make_unique<TestBookmarkClientWithUndo>())) {
    model_->AddObserver(this);
    ClearCounts();
  }

  ~BookmarkModelTest() override { model_->RemoveObserver(this); }

  BookmarkModelTest(const BookmarkModelTest&) = delete;
  BookmarkModelTest& operator=(const BookmarkModelTest&) = delete;

  void BookmarkModelLoaded(bool ids_reassigned) override {
    // We never load from the db, so that this should never get invoked.
    NOTREACHED();
  }

  void OnWillMoveBookmarkNode(const BookmarkNode* old_parent,
                              size_t old_index,
                              const BookmarkNode* new_parent,
                              size_t new_index) override {
    ++before_moved_count_;
    before_observer_details_.Set(old_parent, new_parent, old_index, new_index,
                                 false, FROM_HERE);
  }

  void BookmarkNodeMoved(const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {
    ++moved_count_;
    observer_details_.Set(old_parent, new_parent, old_index, new_index, false,
                          FROM_HERE);
  }

  void BookmarkNodeAdded(const BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {
    ++added_count_;
    observer_details_.Set(parent, nullptr, index, static_cast<size_t>(-1),
                          added_by_user, FROM_HERE);
  }

  void OnWillRemoveBookmarks(const BookmarkNode* parent,
                             size_t old_index,
                             const BookmarkNode* node,
                             const base::Location& location) override {
    ++before_remove_count_;
    before_observer_details_.Set(parent, nullptr, old_index,
                                 static_cast<size_t>(-1), false, location);
  }

  void BookmarkNodeRemoved(const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override {
    ++removed_count_;
    observer_details_.Set(parent, nullptr, old_index, static_cast<size_t>(-1),
                          false, location);
  }

  void BookmarkNodeChanged(const BookmarkNode* node) override {
    ++changed_count_;
    observer_details_.Set(node, nullptr, static_cast<size_t>(-1),
                          static_cast<size_t>(-1), false, FROM_HERE);
  }

  void OnWillChangeBookmarkNode(const BookmarkNode* node) override {
    ++before_change_count_;
    before_observer_details_.Set(node, nullptr, static_cast<size_t>(-1),
                                 static_cast<size_t>(-1), false, FROM_HERE);
  }

  void BookmarkNodeChildrenReordered(const BookmarkNode* node) override {
    ++reordered_count_;
  }

  void OnWillReorderBookmarkNode(const BookmarkNode* node) override {
    ++before_reorder_count_;
  }

  void BookmarkNodeFaviconChanged(const BookmarkNode* node) override {
    // We never attempt to load favicons, so that this method never
    // gets invoked.
  }

  void ExtensiveBookmarkChangesBeginning() override {
    ++extensive_changes_beginning_count_;
  }

  void ExtensiveBookmarkChangesEnded() override {
    ++extensive_changes_ended_count_;
  }

  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override {
    ++all_bookmarks_removed_;
    all_bookmarks_removed_details_.emplace_back(removed_urls);
  }

  void OnWillRemoveAllUserBookmarks(const base::Location& location) override {
    ++before_remove_all_count_;
    observer_details_.Reset();
    before_observer_details_.Reset();
  }

  void GroupedBookmarkChangesBeginning() override {
    ++grouped_changes_beginning_count_;
  }

  void GroupedBookmarkChangesEnded() override {
    ++grouped_changes_ended_count_;
  }

  void ClearCounts() {
    added_count_ = 0;
    moved_count_ = 0;
    removed_count_ = 0;
    changed_count_ = 0;
    reordered_count_ = 0;
    extensive_changes_beginning_count_ = 0;
    extensive_changes_ended_count_ = 0;
    all_bookmarks_removed_ = 0;
    before_moved_count_ = 0;
    before_remove_count_ = 0;
    before_change_count_ = 0;
    before_reorder_count_ = 0;
    before_remove_all_count_ = 0;
    grouped_changes_beginning_count_ = 0;
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
                           int before_remove_all_count,
                           int before_moved_count) {
    EXPECT_EQ(added_count, added_count_);
    EXPECT_EQ(moved_count, moved_count_);
    EXPECT_EQ(removed_count, removed_count_);
    EXPECT_EQ(changed_count, changed_count_);
    EXPECT_EQ(reordered_count, reordered_count_);
    EXPECT_EQ(before_remove_count, before_remove_count_);
    EXPECT_EQ(before_change_count, before_change_count_);
    EXPECT_EQ(before_reorder_count, before_reorder_count_);
    EXPECT_EQ(before_remove_all_count, before_remove_all_count_);
    EXPECT_EQ(before_moved_count_, before_moved_count);
  }

  void AssertExtensiveChangesObserverCount(
      int extensive_changes_beginning_count,
      int extensive_changes_ended_count) {
    EXPECT_EQ(extensive_changes_beginning_count,
              extensive_changes_beginning_count_);
    EXPECT_EQ(extensive_changes_ended_count, extensive_changes_ended_count_);
  }

  void AssertGroupedChangesObserverCount(int grouped_changes_beginning_count,
                                         int grouped_changes_ended_count) {
    EXPECT_EQ(grouped_changes_beginning_count,
              grouped_changes_beginning_count_);
    EXPECT_EQ(grouped_changes_ended_count, grouped_changes_ended_count_);
  }

  int AllNodesRemovedObserverCount() const { return all_bookmarks_removed_; }

  BookmarkPermanentNode* ReloadModelWithManagedNode() {
    model_->RemoveObserver(this);

    auto client = std::make_unique<TestBookmarkClient>();
    BookmarkPermanentNode* managed_node = client->EnableManagedNode();

    model_ = TestBookmarkClient::CreateModelWithClient(std::move(client));
    model_->AddObserver(this);
    ClearCounts();

    if (!model_->root_node()->GetIndexOf(managed_node).has_value()) {
      ADD_FAILURE();
    }

    return managed_node;
  }

  std::vector<const BookmarkPermanentNode*> GetVisiblePermanentNodes() const {
    std::vector<const BookmarkPermanentNode*> visible_nodes;
    for (const auto& node : model_->root_node()->children()) {
      if (node->IsVisible()) {
        visible_nodes.push_back(
            static_cast<const BookmarkPermanentNode*>(node.get()));
      }
    }
    return visible_nodes;
  }

  // For the provided model index, returns the visible index of the permanent
  // node at this index.
  //
  // This can be called in either of the two cases:
  // 1. Steady state, in which case the permanent node at `index` is visible
  // 2. In an onRemoved call for the node, in which case the permanent node was
  //    previously at `index` and visible, but has now been removed.
  size_t GetVisibleIndexForPermanentNode(size_t index) const {
    CHECK_LE(index, model_->root_node()->children().size());
    size_t visible_index = 0;

    for (size_t m_index = 0; m_index < index; ++m_index) {
      const auto& child = model_->root_node()->children()[m_index];
      if (child->IsVisible()) {
        ++visible_index;
      }
    }

    return visible_index;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  base::UserActionTester* user_action_tester() { return &user_action_tester_; }

 protected:
  base::test::ScopedFeatureList features_{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<BookmarkModel> model_;
  ObserverDetails observer_details_;
  ObserverDetails before_observer_details_;
  std::vector<AllNodesRemovedDetail> all_bookmarks_removed_details_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

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
  int before_moved_count_;
  int grouped_changes_beginning_count_;
  int grouped_changes_ended_count_;
};

TEST_F(BookmarkModelTest, InitialState) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  ASSERT_NE(bb_node, nullptr);
  EXPECT_EQ(0u, bb_node->children().size());
  EXPECT_EQ(BookmarkNode::BOOKMARK_BAR, bb_node->type());

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_NE(other_node, nullptr);
  EXPECT_EQ(0u, other_node->children().size());
  EXPECT_EQ(BookmarkNode::OTHER_NODE, other_node->type());

  const BookmarkNode* mobile_node = model_->mobile_node();
  ASSERT_NE(mobile_node, nullptr);
  EXPECT_EQ(0u, mobile_node->children().size());
  EXPECT_EQ(BookmarkNode::MOBILE, mobile_node->type());

  EXPECT_NE(bb_node->id(), other_node->id());
  EXPECT_NE(bb_node->id(), mobile_node->id());
  EXPECT_NE(other_node->id(), mobile_node->id());
}

TEST_F(BookmarkModelTest, AddURL) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  const BookmarkNode* new_node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model_->bookmark_bar_node()->id()),
                             testing::Ne(model_->other_node()->id()),
                             testing::Ne(model_->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddNewURL) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  const BookmarkNode* new_node =
      model_->AddNewURL(bookmark_bar_node, 0, kTitle, kUrl);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), true);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model_->bookmark_bar_node()->id()),
                             testing::Ne(model_->other_node()->id()),
                             testing::Ne(model_->mobile_node()->id())));
}

// Tests recording user action when adding a bookmark in account storage when
// a shared BookmarkModel instance is used for account bookmarks and
// local-or-syncable ones.
TEST_F(BookmarkModelTest,
       AddNewURLAccountStorageOnSharedBookmarkModelInstance) {
  model_->CreateAccountPermanentFolders();
  model_->AddNewURL(model_->account_bookmark_bar_node(), 0, u"title",
                    GURL("http://foo.com"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.Added"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.Added.AccountStorage"));
}

// Tests recording user action when adding a bookmark in local storage not
// syncing.
TEST_F(BookmarkModelTest, AddNewURLLocalStorageNotSyncing) {
  model_->AddNewURL(model_->bookmark_bar_node(), 0, u"title",
                    GURL("http://foo.com"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.Added"));
  EXPECT_EQ(
      1, user_action_tester()->GetActionCount("Bookmarks.Added.LocalStorage"));
}

// Tests recording user action when adding a bookmark in local storage syncing.
TEST_F(BookmarkModelTest, AddNewURLLocalStorageSyncing) {
  static_cast<TestBookmarkClient*>(model_->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  model_->AddNewURL(model_->bookmark_bar_node(), 0, u"title",
                    GURL("http://foo.com"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.Added"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.Added.LocalStorageSyncing"));
}

// Tests recording user action when adding a folder in account storage.
TEST_F(BookmarkModelTest, AddNewFolderAccountStorage) {
  model_->CreateAccountPermanentFolders();

  model_->AddFolder(model_->account_mobile_node(), 0, u"title");
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.FolderAdded"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.FolderAdded.AccountStorage"));
}

// Tests recording user action when adding a folder in local storage not
// syncing.
TEST_F(BookmarkModelTest, AddNewFolderLocalStorageNotSyncing) {
  model_->AddFolder(model_->mobile_node(), 0, u"title");
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.FolderAdded"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.FolderAdded.LocalStorage"));
}

// Tests recording user action when adding a folder in local storage syncing.
TEST_F(BookmarkModelTest, AddNewFolderLocalStorageSyncing) {
  static_cast<TestBookmarkClient*>(model_->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  model_->AddFolder(model_->mobile_node(), 0, u"title");
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.FolderAdded"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.FolderAdded.LocalStorageSyncing"));
}

TEST_F(BookmarkModelTest, AddURLWithUnicodeTitle) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(
      u"\u767e\u5ea6\u4e00\u4e0b\uff0c\u4f60\u5c31\u77e5\u9053");
  const GURL kUrl("https://www.baidu.com/");

  const BookmarkNode* new_node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model_->bookmark_bar_node()->id()),
                             testing::Ne(model_->other_node()->id()),
                             testing::Ne(model_->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddURLWithWhitespaceTitle) {
  for (size_t i = 0; i < std::size(kUrlWhitespaceTestCases); ++i) {
    const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
    const std::u16string kTitle(
        ASCIIToUTF16(kUrlWhitespaceTestCases[i].input_title));
    const GURL kUrl("http://foo.com");

    const BookmarkNode* new_node =
        model_->AddURL(bookmark_bar_node, i, kTitle, kUrl);

    EXPECT_EQ(i + 1, bookmark_bar_node->children().size());
    EXPECT_EQ(ASCIIToUTF16(kUrlWhitespaceTestCases[i].expected_title),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::URL, new_node->type());
  }
}

TEST_F(BookmarkModelTest, AddURLWithCreationTimeAndMetaInfo) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const Time kTime = Time::Now() - base::Days(1);
  BookmarkNode::MetaInfoMap meta_info;
  meta_info["foo"] = "bar";

  const BookmarkNode* new_node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl, &meta_info, kTime);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(kTime, new_node->date_added());
  ASSERT_TRUE(new_node->GetMetaInfoMap());
  ASSERT_EQ(meta_info, *new_node->GetMetaInfoMap());
  ASSERT_EQ(new_node, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model_->bookmark_bar_node()->id()),
                             testing::Ne(model_->other_node()->id()),
                             testing::Ne(model_->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddURLWithUUID) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const Time kTime = Time::Now() - base::Days(1);
  BookmarkNode::MetaInfoMap meta_info;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  const BookmarkNode* new_node = model_->AddURL(
      bookmark_bar_node, /*index=*/0, kTitle, kUrl, &meta_info, kTime, kUuid);

  EXPECT_EQ(kUuid, new_node->uuid());
}

TEST_F(BookmarkModelTest, AddURLToMobileBookmarks) {
  const BookmarkNode* mobile_node = model_->mobile_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  const BookmarkNode* new_node = model_->AddURL(mobile_node, 0, kTitle, kUrl);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(mobile_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);

  ASSERT_EQ(1u, mobile_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model_->bookmark_bar_node()->id()),
                             testing::Ne(model_->other_node()->id()),
                             testing::Ne(model_->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddFolder) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");

  const BookmarkNode* new_node =
      model_->AddFolder(bookmark_bar_node, 0, kTitle);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::FOLDER, new_node->type());

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model_->bookmark_bar_node()->id()),
                             testing::Ne(model_->other_node()->id()),
                             testing::Ne(model_->mobile_node()->id())));

  // Add another folder, just to make sure folder_ids are incremented correctly.
  ClearCounts();
  model_->AddFolder(bookmark_bar_node, 0, kTitle);
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);
}

TEST_F(BookmarkModelTest, AddFolderWithCreationTime) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  BookmarkNode::MetaInfoMap meta_info;
  const base::Time kCreationTime(base::Time::Now() - base::Days(1));

  const BookmarkNode* new_node = model_->AddFolder(
      bookmark_bar_node, /*index=*/0, kTitle, &meta_info, kCreationTime);

  EXPECT_EQ(kCreationTime, new_node->date_added());
}

TEST_F(BookmarkModelTest, AddFolderWithUUID) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  BookmarkNode::MetaInfoMap meta_info;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  const BookmarkNode* new_node =
      model_->AddFolder(bookmark_bar_node, /*index=*/0, kTitle, &meta_info,
                        /*creation_time=*/Time::Now(), kUuid);

  EXPECT_EQ(kUuid, new_node->uuid());
}

TEST_F(BookmarkModelTest, AddFolderWithWhitespaceTitle) {
  for (size_t i = 0; i < std::size(kTitleWhitespaceTestCases); ++i) {
    const auto& test_case = kTitleWhitespaceTestCases[i];
    const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
    const std::u16string kTitle(ASCIIToUTF16(test_case.input_title));

    const BookmarkNode* new_node =
        model_->AddFolder(bookmark_bar_node, i, kTitle);

    EXPECT_EQ(i + 1, bookmark_bar_node->children().size());
    EXPECT_EQ(ASCIIToUTF16(test_case.expected_title), new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  }
}

TEST_F(BookmarkModelTest, RemoveURL) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const base::Location kLocation = FROM_HERE;

  model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  ClearCounts();

  model_->Remove(bookmark_bar_node->children().front().get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, kLocation);
  ASSERT_EQ(0u, bookmark_bar_node->children().size());
  histogram_tester()->ExpectTotalCount("Bookmarks.RemovedSource", 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.RemovedSource",
      static_cast<int>(metrics::BookmarkEditSource::kOther), 1);
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false, kLocation);

  // Make sure there is no mapping for the URL.
  ASSERT_EQ(model_->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);
}

TEST_F(BookmarkModelTest, RemoveFolder) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* folder = model_->AddFolder(bookmark_bar_node, 0, u"foo");

  ClearCounts();

  // Add a URL as a child.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  model_->AddURL(folder, 0, kTitle, kUrl);

  ClearCounts();

  // Now remove the folder.
  model_->Remove(bookmark_bar_node->children().front().get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  ASSERT_EQ(0u, bookmark_bar_node->children().size());
  histogram_tester()->ExpectTotalCount("Bookmarks.RemovedSource", 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.RemovedSource",
      static_cast<int>(metrics::BookmarkEditSource::kOther), 1);
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);

  // Make sure there is no mapping for the URL.
  ASSERT_EQ(model_->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);
}

TEST_F(BookmarkModelTest, RemoveLastChild) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle1(u"foo1");
  const std::u16string kTitle2(u"foo2");
  const GURL kUrl1("http://foo1.com");
  const GURL kUrl2("http://foo2.com");
  const base::Location kLocation = FROM_HERE;

  model_->AddURL(bookmark_bar_node, 0, kTitle1, kUrl1);
  model_->AddURL(bookmark_bar_node, 1, kTitle2, kUrl2);
  ClearCounts();

  model_->RemoveLastChild(bookmark_bar_node,
                          bookmarks::metrics::BookmarkEditSource::kOther,
                          kLocation);
  EXPECT_EQ(1u, bookmark_bar_node->children().size());
  histogram_tester()->ExpectTotalCount("Bookmarks.RemovedSource", 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.RemovedSource",
      static_cast<int>(metrics::BookmarkEditSource::kOther), 1);
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 1,
                                 static_cast<size_t>(-1), false, kLocation);

  ASSERT_NE(nullptr, model_->GetMostRecentlyAddedUserNodeForURL(kUrl1));
  // Make sure there is no mapping for the removed URL.
  EXPECT_EQ(nullptr, model_->GetMostRecentlyAddedUserNodeForURL(kUrl2));
}

TEST_F(BookmarkModelTest, RemoveAllUserBookmarks) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  ClearCounts();

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  // Add a folder with child URL.
  const BookmarkNode* folder = model_->AddFolder(bookmark_bar_node, 0, kTitle);
  model_->AddURL(folder, 0, kTitle, kUrl);

  AssertObserverCount(3, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  ClearCounts();

  size_t permanent_node_count = model_->root_node()->children().size();

  model_->RemoveAllUserBookmarks(FROM_HERE);

  EXPECT_EQ(0u, bookmark_bar_node->children().size());
  // No permanent node should be removed.
  EXPECT_EQ(permanent_node_count, model_->root_node()->children().size());
  // No individual BookmarkNodeRemoved events are fired, so removed count
  // should be 0.
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 1, 0);
  AssertExtensiveChangesObserverCount(1, 1);
  AssertGroupedChangesObserverCount(1, 1);
  EXPECT_EQ(1, AllNodesRemovedObserverCount());
  EXPECT_EQ(1, AllNodesRemovedObserverCount());
  EXPECT_THAT(all_bookmarks_removed_details_,
              ElementsAre(AllNodesRemovedDetail(/*removed_urls=*/{kUrl})));
}

TEST_F(BookmarkModelTest, UpdateLastUsedTimeInRange) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  ClearCounts();

  const base::Time kAddedTime = base::Time::Now();
  const base::Time kUsedTime1 = kAddedTime + base::Days(2);

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* url_node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl, nullptr, kAddedTime);
  model_->UpdateLastUsedTime(url_node, kUsedTime1, /*just_opened=*/true);
  EXPECT_EQ(kUsedTime1, url_node->date_last_used());
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceLastUsed", 0);
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceAdded", 1);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceAdded", 2,
                                        1);

  const base::Time kUsedTime2 = kAddedTime + base::Days(7);
  model_->UpdateLastUsedTime(url_node, kUsedTime2, /*just_opened=*/true);
  EXPECT_EQ(kUsedTime2, url_node->date_last_used());
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceLastUsed", 1);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceLastUsed", 5,
                                        1);
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceAdded", 2);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceAdded", 2,
                                        1);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceAdded", 7,
                                        1);

  // This update isn't a result of an open, but rather a sync event.
  // The value should update while the histogram count should remain the same.
  base::Time kUsedTime3 = kAddedTime + base::Days(7);
  model_->UpdateLastUsedTime(url_node, kUsedTime3, /*just_opened=*/false);
  EXPECT_EQ(kUsedTime3, url_node->date_last_used());
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceLastUsed", 1);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceLastUsed", 5,
                                        1);
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceAdded", 2);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceAdded", 2,
                                        1);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceAdded", 7,
                                        1);
}

TEST_F(BookmarkModelTest, ClearLastUsedTimeInRange) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  ClearCounts();

  const base::Time kTime = base::Time::Now();

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* url_node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  model_->UpdateLastUsedTime(url_node, kTime, /*just_opened=*/true);

  // Add a folder with child URL.
  const BookmarkNode* folder = model_->AddFolder(bookmark_bar_node, 0, kTitle);
  const BookmarkNode* folder_url_node = model_->AddURL(folder, 0, kTitle, kUrl);
  model_->UpdateLastUsedTime(folder_url_node, kTime, /*just_opened=*/true);
  EXPECT_EQ(kTime, url_node->date_last_used());
  EXPECT_EQ(kTime, folder_url_node->date_last_used());

  model_->ClearLastUsedTimeInRange(kTime - base::Seconds(1),
                                   kTime + base::Seconds(1));
  EXPECT_EQ(base::Time(), url_node->date_last_used());
  EXPECT_EQ(base::Time(), folder_url_node->date_last_used());
}

TEST_F(BookmarkModelTest, ClearLastUsedTimeInRangeForAllTime) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  ClearCounts();

  const base::Time kTime = base::Time::Now();

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* url_node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  model_->UpdateLastUsedTime(url_node, kTime, /*just_opened=*/true);

  // Add a folder with child URL.
  const BookmarkNode* folder = model_->AddFolder(bookmark_bar_node, 0, kTitle);
  const BookmarkNode* folder_url_node = model_->AddURL(folder, 0, kTitle, kUrl);
  model_->UpdateLastUsedTime(folder_url_node, kTime, /*just_opened=*/true);
  EXPECT_EQ(kTime, url_node->date_last_used());
  EXPECT_EQ(kTime, folder_url_node->date_last_used());

  model_->ClearLastUsedTimeInRange(base::Time(), base::Time::Max());
  EXPECT_EQ(base::Time(), url_node->date_last_used());
  EXPECT_EQ(base::Time(), folder_url_node->date_last_used());
}

TEST_F(BookmarkModelTest, SetTitle) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kOriginalTitle(u"foo");
  const GURL kUrl("http://url.com");
  const BookmarkNode* node =
      model_->AddURL(bookmark_bar_node, 0, kOriginalTitle, kUrl);

  ClearCounts();

  const std::u16string kNewTitle(u"goo");
  model_->SetTitle(node, kNewTitle, metrics::BookmarkEditSource::kOther);
  AssertObserverCount(0, 0, 0, 1, 0, 0, 1, 0, 0, 0);
  observer_details_.ExpectEquals(node, nullptr, static_cast<size_t>(-1),
                                 static_cast<size_t>(-1), false);
  EXPECT_EQ(kNewTitle, node->GetTitle());

  // Should update the index.
  auto matches =
      model_->GetBookmarksMatching(kOriginalTitle, /*max_count=*/1,
                                   query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model_->GetBookmarksMatching(
      kNewTitle, /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(kUrl, matches[0].node->GetTitledUrlNodeUrl());
  histogram_tester()->ExpectBucketCount("Bookmarks.EditTitleSource",
                                        metrics::BookmarkEditSource::kOther, 1);
}

TEST_F(BookmarkModelTest, SetTitleWithWhitespace) {
  for (const auto& test_case : kTitleWhitespaceTestCases) {
    const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
    const GURL kUrl("http://foo.com");
    const BookmarkNode* node =
        model_->AddURL(bookmark_bar_node, 0, u"dummy", kUrl);

    const std::u16string title = ASCIIToUTF16(test_case.input_title);
    model_->SetTitle(node, title, metrics::BookmarkEditSource::kOther);
    EXPECT_EQ(ASCIIToUTF16(test_case.expected_title), node->GetTitle());
  }
}

TEST_F(BookmarkModelTest, SetFolderTitle) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* folder =
      model_->AddFolder(bookmark_bar_node, 0, u"folder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model_->AddURL(folder, 0, kTitle, kUrl);

  ClearCounts();

  model_->SetTitle(folder, u"golder", metrics::BookmarkEditSource::kOther);

  // Should not change the hierarchy.
  EXPECT_EQ(bookmark_bar_node->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children().front().get(), folder);
  EXPECT_EQ(folder->children().size(), 1u);
  EXPECT_EQ(folder->children().front().get(), node);
  EXPECT_EQ(node->parent(), folder);

  // Should update the index.
  auto matches = model_->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model_->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].node, node);
  EXPECT_EQ(matches[0].node->GetTitledUrlNodeUrl(), kUrl);
  histogram_tester()->ExpectBucketCount("Bookmarks.EditTitleSource",
                                        metrics::BookmarkEditSource::kOther, 1);
}

TEST_F(BookmarkModelTest, SetURL) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kOriginalUrl("http://foo.com");
  const BookmarkNode* node =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kOriginalUrl);

  ClearCounts();

  const GURL kNewUrl("http://foo2.com");
  model_->SetURL(node, kNewUrl, metrics::BookmarkEditSource::kOther);
  AssertObserverCount(0, 0, 0, 1, 0, 0, 1, 0, 0, 0);
  observer_details_.ExpectEquals(node, nullptr, static_cast<size_t>(-1),
                                 static_cast<size_t>(-1), false);
  EXPECT_EQ(kNewUrl, node->url());
  histogram_tester()->ExpectBucketCount("Bookmarks.EditURLSource",
                                        metrics::BookmarkEditSource::kOther, 1);
}

TEST_F(BookmarkModelTest, SetDateAdded) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  ClearCounts();

  const base::Time kNewTime = base::Time::Now() + base::Minutes(20);
  model_->SetDateAdded(node, kNewTime);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(kNewTime, node->date_added());
  EXPECT_EQ(kNewTime, model_->bookmark_bar_node()->date_folder_modified());
}

TEST_F(BookmarkModelTest, Move) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  const BookmarkNode* folder1 =
      model_->AddFolder(bookmark_bar_node, 0, u"folder");

  ClearCounts();

  model_->Move(node, folder1, 0);

  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(bookmark_bar_node, folder1, 1, 0,
                                        false);
  observer_details_.ExpectEquals(bookmark_bar_node, folder1, 1, 0, false);
  EXPECT_EQ(folder1, node->parent());
  EXPECT_EQ(1u, bookmark_bar_node->children().size());
  EXPECT_EQ(folder1, bookmark_bar_node->children().front().get());
  EXPECT_EQ(1u, folder1->children().size());
  EXPECT_EQ(node, folder1->children().front().get());

  auto matches = model_->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);

  // And remove the folder.
  ClearCounts();
  model_->Remove(bookmark_bar_node->children().front().get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  AssertObserverCount(0, 0, 1, 0, 0, 1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(bookmark_bar_node, nullptr, 0,
                                 static_cast<size_t>(-1), false);
  EXPECT_EQ(model_->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);
  EXPECT_EQ(0u, bookmark_bar_node->children().size());

  matches = model_->GetBookmarksMatching(
      u"foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
}

TEST_F(BookmarkModelTest, MoveToSameParent) {
  TestNode abc;
  PopulateNodeFromString("A B C", &abc);
  TestNode bac;
  PopulateNodeFromString("B A C", &bac);
  TestNode bca;
  PopulateNodeFromString("B C A", &bca);

  // Populate the parent with [a, b, c].
  BookmarkNode* parent = AsMutable(model_->bookmark_bar_node());
  PopulateBookmarkNode(&abc, model_.get(), parent);

  // Move to current_index - 1 moves to the left: [a, b, c] -> [b, a, c]
  ClearCounts();
  model_->Move(parent->children()[1].get(), parent, 0);
  VerifyModelMatchesNode(&bac, parent);
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(parent, parent, 1, 0, false);
  observer_details_.ExpectEquals(parent, parent, 1, 0, false);

  // Move the current_index is a no-op: [b, a, c] -> [b, a, c]
  ClearCounts();
  model_->Move(parent->children()[1].get(), parent, 1);
  VerifyModelMatchesNode(&bac, parent);
  AssertObserverCount(0, /*moved_count=*/0, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/0);

  // Move to current_index + 1 is a no-op: [b, a, c] -> [b, a, c]
  ClearCounts();
  model_->Move(parent->children()[1].get(), parent, 2);
  VerifyModelMatchesNode(&bac, parent);
  AssertObserverCount(0, /*moved_count=*/0, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/0);

  // Move to current_index + 2 moves 1 to the right: [b, a, c] -> [b, c, a]
  // Note: this tests moving to an index that is one past the end of the list.
  ClearCounts();
  model_->Move(parent->children()[1].get(), parent, 3);
  VerifyModelMatchesNode(&bca, parent);
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(parent, parent, 1, 2, false);
  observer_details_.ExpectEquals(parent, parent, 1, 2, false);
}

TEST_F(BookmarkModelTest, NonMovingMoveCall) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const base::Time kOldDate(base::Time::Now() - base::Days(1));

  const BookmarkNode* node = model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  model_->SetDateFolderModified(bookmark_bar_node, kOldDate);

  // Since `node` is already at the index 0 of `bookmark_bar_node`, this is
  // no-op.
  model_->Move(node, bookmark_bar_node, 0);

  // Check that the modification date is kept untouched.
  EXPECT_EQ(kOldDate, bookmark_bar_node->date_folder_modified());
}

TEST_F(BookmarkModelTest, MoveURLFromFolder) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* folder1 =
      model_->AddFolder(bookmark_bar_node, 0, u"folder");
  const BookmarkNode* folder2 =
      model_->AddFolder(bookmark_bar_node, 0, u"golder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model_->AddURL(folder1, 0, kTitle, kUrl);

  ClearCounts();

  model_->Move(node, folder2, 0);

  // Should update the hierarchy.
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(folder1, folder2, 0, 0, false);
  observer_details_.ExpectEquals(folder1, folder2, 0, 0, false);
  EXPECT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(folder1->children().size(), 0u);
  EXPECT_EQ(folder2->children().size(), 1u);
  EXPECT_EQ(folder2->children().front().get(), node);

  auto matches = model_->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model_->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();

  // Move back.
  ClearCounts();
  model_->Move(node, folder1, 0);

  // Should update the hierarchy.
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(folder2, folder1, 0, 0, false);
  observer_details_.ExpectEquals(folder2, folder1, 0, 0, false);
  EXPECT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(folder1->children().size(), 1u);
  EXPECT_EQ(folder2->children().size(), 0u);
  EXPECT_EQ(folder1->children().front().get(), node);

  matches = model_->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();
  matches = model_->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
}

TEST_F(BookmarkModelTest, MoveFolder) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* folder1 =
      model_->AddFolder(bookmark_bar_node, 0, u"folder");
  const BookmarkNode* folder2 =
      model_->AddFolder(bookmark_bar_node, 1, u"golder");
  const BookmarkNode* folder3 = model_->AddFolder(folder1, 0, u"holder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model_->AddURL(folder3, 0, kTitle, kUrl);

  ClearCounts();

  model_->Move(folder3, folder2, 0);

  // Should update the hierarchy.
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(folder1, folder2, 0, 0, false);
  observer_details_.ExpectEquals(folder1, folder2, 0, 0, false);
  EXPECT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0].get(), folder1);
  EXPECT_EQ(bookmark_bar_node->children()[1].get(), folder2);
  EXPECT_EQ(folder1->children().size(), 0u);
  EXPECT_EQ(folder2->children().size(), 1u);
  EXPECT_EQ(folder2->children()[0].get(), folder3);
  EXPECT_EQ(folder3->children().size(), 1u);
  EXPECT_EQ(folder3->children()[0].get(), node);

  // Should update the index.
  auto matches = model_->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model_->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();
  matches = model_->GetBookmarksMatching(
      u"holder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();
}

TEST_F(BookmarkModelTest, MoveWithUuidCollision) {
  model_->CreateAccountPermanentFolders();
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* account_bookmark_bar_node =
      model_->account_bookmark_bar_node();

  ASSERT_NE(nullptr, account_bookmark_bar_node);

  // Create two folders with the same UUID. One is local and the other one is an
  // account bookmark, so using the same UUID is allowed.
  const BookmarkNode* folder1 =
      model_->AddFolder(bookmark_bar_node, 0, u"local folder");
  const base::Uuid kInitialUuid = folder1->uuid();
  const BookmarkNode* folder2 =
      model_->AddFolder(account_bookmark_bar_node, 0, u"account folder",
                        /*meta_info=*/nullptr,
                        /*creation_time=*/std::nullopt, kInitialUuid);

  ClearCounts();

  model_->Move(folder1, account_bookmark_bar_node, 0);

  // Should update the hierarchy.
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(bookmark_bar_node,
                                        account_bookmark_bar_node, 0, 0, false);
  observer_details_.ExpectEquals(bookmark_bar_node, account_bookmark_bar_node,
                                 0, 0, false);
  EXPECT_EQ(bookmark_bar_node->children().size(), 0u);
  EXPECT_EQ(account_bookmark_bar_node->children().size(), 2u);

  // The UUID should have changed for the moved folder.
  EXPECT_NE(folder1->uuid(), kInitialUuid);

  // The other folder involved in the collision should continue having the
  // original UUID.
  EXPECT_EQ(folder2->uuid(), kInitialUuid);

  // Verify that the UUID index was updated correctly.
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(
                kInitialUuid, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder2, model_->GetNodeByUuid(
                         kInitialUuid, NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder1,
            model_->GetNodeByUuid(folder1->uuid(),
                                  NodeTypeForUuidLookup::kAccountNodes));
}

TEST_F(BookmarkModelTest, MoveWithUuidCollisionInDescendant) {
  model_->CreateAccountPermanentFolders();
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* account_bookmark_bar_node =
      model_->account_bookmark_bar_node();

  ASSERT_NE(nullptr, account_bookmark_bar_node);

  const BookmarkNode* folder1 =
      model_->AddFolder(bookmark_bar_node, 0, u"local folder");
  const base::Uuid kFolder1Uuid = folder1->uuid();

  // Create two more folders with the same UUID. One is local (nested under
  // `folder1`) and the other one is an account bookmark, so using the same UUID
  // is allowed.
  const BookmarkNode* folder2 = model_->AddFolder(folder1, 0, u"local folder");
  const base::Uuid kInitialUuid = folder2->uuid();
  const BookmarkNode* folder3 =
      model_->AddFolder(account_bookmark_bar_node, 0, u"account folder",
                        /*meta_info=*/nullptr,
                        /*creation_time=*/std::nullopt, kInitialUuid);

  ClearCounts();

  // Move `folder1`, which also includes the nested `folder2`, to account
  // storage.
  model_->Move(folder1, account_bookmark_bar_node, 0);

  // Should update the hierarchy.
  AssertObserverCount(0, /*moved_count=*/1, 0, 0, 0, 0, 0, 0, 0,
                      /*before_moved_count=*/1);
  before_observer_details_.ExpectEquals(bookmark_bar_node,
                                        account_bookmark_bar_node, 0, 0, false);
  observer_details_.ExpectEquals(bookmark_bar_node, account_bookmark_bar_node,
                                 0, 0, false);
  EXPECT_EQ(bookmark_bar_node->children().size(), 0u);
  EXPECT_EQ(account_bookmark_bar_node->children().size(), 2u);

  // The UUID should have changed for `folder2`, but not for `folder1`.
  EXPECT_EQ(folder1->uuid(), kFolder1Uuid);
  EXPECT_NE(folder2->uuid(), kInitialUuid);

  // The other folder involved in the collision should continue having the
  // original UUID.
  EXPECT_EQ(folder3->uuid(), kInitialUuid);

  // Verify that the UUID index was updated correctly.
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(
                kInitialUuid, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder3, model_->GetNodeByUuid(
                         kInitialUuid, NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder1, model_->GetNodeByUuid(
                         kFolder1Uuid, NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder2,
            model_->GetNodeByUuid(folder2->uuid(),
                                  NodeTypeForUuidLookup::kAccountNodes));
}

// Tests the default node if no bookmarks have been added yet
TEST_F(BookmarkModelTest, ParentForNewNodesWithEmptyModel) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(model_->mobile_node(), GetParentForNewNodes(model_.get()));
#else
  EXPECT_EQ(model_->other_node(), GetParentForNewNodes(model_.get()));
#endif
}

#if BUILDFLAG(IS_ANDROID)
// Tests that the bookmark_bar_node can still be returned even on Android in
// case the last bookmark was added to it.
TEST_F(BookmarkModelTest, ParentCanBeBookmarkBarOnAndroid) {
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model_->AddURL(model_->bookmark_bar_node(), 0, kTitle, kUrl);
  EXPECT_EQ(model_->bookmark_bar_node(), GetParentForNewNodes(model_.get()));
}
#endif

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewNodes) {
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model_->AddURL(model_->other_node(), 0, kTitle, kUrl);
  EXPECT_EQ(model_->other_node(), GetParentForNewNodes(model_.get()));
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewMobileNodes) {
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model_->AddURL(model_->mobile_node(), 0, kTitle, kUrl);
  EXPECT_EQ(model_->mobile_node(), GetParentForNewNodes(model_.get()));
}

// Make sure recently modified stays in sync when adding a URL.
TEST_F(BookmarkModelTest, MostRecentlyModifiedFolders) {
  // Add a folder.
  const BookmarkNode* folder =
      model_->AddFolder(model_->other_node(), 0, u"foo");
  // Add a URL to it.
  model_->AddURL(folder, 0, u"blah", GURL("http://foo.com"));

  // Make sure folder is in the most recently modified.
  std::vector<const BookmarkNode*> most_recent_folders =
      GetMostRecentlyModifiedUserFolders(model_.get());
  ASSERT_FALSE(most_recent_folders.empty());
  EXPECT_EQ(folder, most_recent_folders[0]);

  // Nuke the folder and do another fetch, making sure folder isn't in the
  // returned list.
  model_->Remove(folder->parent()->children().front().get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  most_recent_folders = GetMostRecentlyModifiedUserFolders(model_.get());
  EXPECT_FALSE(base::Contains(most_recent_folders, folder));
}

// Make sure MostRecentlyAddedEntries stays in sync.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  const Time kBaseTime = Time::Now();
  BookmarkNode* n1 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 0, u"blah", GURL("http://foo.com/0")));
  BookmarkNode* n2 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 1, u"blah", GURL("http://foo.com/1")));
  BookmarkNode* n3 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 2, u"blah", GURL("http://foo.com/2")));
  BookmarkNode* n4 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 3, u"blah", GURL("http://foo.com/3")));
  n1->set_date_added(kBaseTime + base::Days(4));
  n2->set_date_added(kBaseTime + base::Days(3));
  n3->set_date_added(kBaseTime + base::Days(2));
  n4->set_date_added(kBaseTime + base::Days(1));

  // Make sure order is honored.
  std::vector<const BookmarkNode*> recently_added;
  GetMostRecentlyAddedEntries(model_.get(), 2, &recently_added);
  EXPECT_THAT(recently_added, ElementsAre(n1, n2));

  // swap 1 and 2, then check again.
  recently_added.clear();
  SwapDateAdded(n1, n2);
  GetMostRecentlyAddedEntries(model_.get(), 4, &recently_added);
  EXPECT_THAT(recently_added, ElementsAre(n2, n1, n3, n4));
}

// Make sure MostRecentlyAddedEntries applies across local and account
// bookmarks.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntriesLocalAndAccountBookmarks) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model_->CreateAccountPermanentFolders();

  // Add nodes such that the following holds for the creation time of the
  // nodes: n1 > n2 > n3.
  const Time kBaseTime = Time::Now();
  BookmarkNode* n1 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 0, u"blah", GURL("http://foo.com/1")));
  BookmarkNode* n2 =
      AsMutable(model_->AddURL(model_->account_bookmark_bar_node(), 0, u"blah",
                               GURL("http://foo.com/2")));
  BookmarkNode* n3 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 1, u"blah", GURL("http://foo.com/3")));
  n1->set_date_added(kBaseTime + base::Days(3));
  n2->set_date_added(kBaseTime + base::Days(2));
  n3->set_date_added(kBaseTime + base::Days(1));

  // Make sure order is honored across storages.
  std::vector<const BookmarkNode*> recently_added;
  GetMostRecentlyAddedEntries(model_.get(), 3, &recently_added);
  EXPECT_THAT(recently_added, ElementsAre(n1, n2, n3));
}

// Make sure GetMostRecentlyUsedEntries stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyUsedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  const Time kBaseTime = Time::Now();
  BookmarkNode* n1 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 0, u"blah", GURL("http://foo.com/0")));
  BookmarkNode* n2 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 1, u"blah", GURL("http://foo.com/1")));
  BookmarkNode* n3 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 2, u"blah", GURL("http://foo.com/2")));
  BookmarkNode* n4 = AsMutable(model_->AddURL(
      model_->bookmark_bar_node(), 3, u"blah", GURL("http://foo.com/3")));
  n1->set_date_last_used(kBaseTime + base::Days(4));
  n2->set_date_last_used(kBaseTime + base::Days(3));
  n3->set_date_last_used(kBaseTime + base::Days(2));
  n3->set_date_added(kBaseTime + base::Days(2));
  n4->set_date_last_used(kBaseTime + base::Days(2));
  n4->set_date_added(kBaseTime + base::Days(1));

  // Make sure order is honored.
  std::vector<const BookmarkNode*> recently_used;
  GetMostRecentlyUsedEntries(model_.get(), 2, &recently_used);
  EXPECT_THAT(recently_used, ElementsAre(n1, n2));

  // swap 1 and 2, then check again.
  recently_used.clear();
  SwapDateUsed(n1, n2);
  GetMostRecentlyUsedEntries(model_.get(), 4, &recently_used);
  EXPECT_THAT(recently_used, ElementsAre(n2, n1, n3, n4));
}

// Makes sure GetMostRecentlyAddedUserNodeForURL stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedUserNodeForURL) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2
  const Time kBaseTime = Time::Now();
  const GURL kUrl("http://foo.com/0");
  BookmarkNode* n1 =
      AsMutable(model_->AddURL(model_->bookmark_bar_node(), 0, u"blah", kUrl));
  BookmarkNode* n2 =
      AsMutable(model_->AddURL(model_->bookmark_bar_node(), 1, u"blah", kUrl));
  n1->set_date_added(kBaseTime + base::Days(4));
  n2->set_date_added(kBaseTime + base::Days(3));

  // Make sure order is honored.
  ASSERT_EQ(n1, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  // swap 1 and 2, then check again.
  SwapDateAdded(n1, n2);
  ASSERT_EQ(n2, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));
}

// Makes sure GetUniqueUrls removes duplicates.
TEST_F(BookmarkModelTest, GetUniqueUrlsWithDups) {
  const GURL kUrl("http://foo.com/0");
  const std::u16string kTitle(u"blah");
  model_->AddURL(model_->bookmark_bar_node(), 0, kTitle, kUrl);
  model_->AddURL(model_->bookmark_bar_node(), 1, kTitle, kUrl);

  std::vector<UrlAndTitle> bookmarks = model_->GetUniqueUrls();
  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_EQ(kUrl, bookmarks[0].url);
  EXPECT_EQ(kTitle, bookmarks[0].title);

  model_->AddURL(model_->bookmark_bar_node(), 2, u"Title2", kUrl);
  // Only one returned, even titles are different.
  bookmarks = model_->GetUniqueUrls();
  EXPECT_EQ(1U, bookmarks.size());
}

TEST_F(BookmarkModelTest, HasBookmarks) {
  const GURL kUrl("http://foo.com/");
  model_->AddURL(model_->bookmark_bar_node(), 0, u"bar", kUrl);

  EXPECT_TRUE(model_->HasBookmarks());
}

TEST_F(BookmarkModelTest, Sort) {
  // Populate the bookmark bar node with nodes for 'B', 'a', 'd' and 'C'.
  // 'C' and 'a' are folders.
  TestNode bbn;
  PopulateNodeFromString("B [ a ] d [ a ]", &bbn);
  const BookmarkNode* parent = model_->bookmark_bar_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);

  BookmarkNode* child1 = parent->children()[1].get();
  child1->SetTitle(u"a");
  child1->Remove(0);
  BookmarkNode* child3 = parent->children()[3].get();
  child3->SetTitle(u"C");
  child3->Remove(0);

  ClearCounts();

  // Sort the children of the bookmark bar node.
  model_->SortChildren(parent);

  // Make sure we were notified.
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 1, 0, 0);

  // Make sure the order matches (remember, 'a' and 'C' are folders and
  // come first).
  EXPECT_EQ(parent->children()[0]->GetTitle(), u"a");
  EXPECT_EQ(parent->children()[1]->GetTitle(), u"C");
  EXPECT_EQ(parent->children()[2]->GetTitle(), u"B");
  EXPECT_EQ(parent->children()[3]->GetTitle(), u"d");
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
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 1, 0, 0);

  // Make sure the order matches is correct (it should be reversed).
  ASSERT_EQ(4u, parent->children().size());
  EXPECT_EQ("D", base::UTF16ToASCII(parent->children()[0]->GetTitle()));
  EXPECT_EQ("C", base::UTF16ToASCII(parent->children()[1]->GetTitle()));
  EXPECT_EQ("B", base::UTF16ToASCII(parent->children()[2]->GetTitle()));
  EXPECT_EQ("A", base::UTF16ToASCII(parent->children()[3]->GetTitle()));
}

TEST_F(BookmarkModelTest, NoOpReorderCall) {
  // Populate the bookmark bar node with nodes 'A', 'B', 'C' and 'D'.
  TestNode bbn;
  PopulateNodeFromString("A B C D", &bbn);
  BookmarkNode* parent = AsMutable(model_->bookmark_bar_node());
  PopulateBookmarkNode(&bbn, model_.get(), parent);

  ClearCounts();

  std::vector<const BookmarkNode*> same_order = {
      parent->children()[0].get(),
      parent->children()[1].get(),
      parent->children()[2].get(),
      parent->children()[3].get(),
  };
  model_->ReorderChildren(parent, same_order);

  // Make sure observers were not notified.
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // Make sure the order remains the same.
  ASSERT_EQ(4u, parent->children().size());
  EXPECT_EQ("A", base::UTF16ToASCII(parent->children()[0]->GetTitle()));
  EXPECT_EQ("B", base::UTF16ToASCII(parent->children()[1]->GetTitle()));
  EXPECT_EQ("C", base::UTF16ToASCII(parent->children()[2]->GetTitle()));
  EXPECT_EQ("D", base::UTF16ToASCII(parent->children()[3]->GetTitle()));
}

TEST_F(BookmarkModelTest, ReorderCallWithSizeMismatch) {
  // Populate the bookmark bar node with nodes 'A', 'B', 'C' and 'D'.
  TestNode bbn;
  PopulateNodeFromString("A B C D", &bbn);
  BookmarkNode* parent = AsMutable(model_->bookmark_bar_node());
  PopulateBookmarkNode(&bbn, model_.get(), parent);

  ClearCounts();

  std::vector<const BookmarkNode*> order_with_size_mismatch = {
      parent->children()[1].get(),
  };
  model_->ReorderChildren(parent, order_with_size_mismatch);

  // Make sure observers were not notified.
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // Make sure the order remains the same.
  ASSERT_EQ(4u, parent->children().size());
  EXPECT_EQ("A", base::UTF16ToASCII(parent->children()[0]->GetTitle()));
  EXPECT_EQ("B", base::UTF16ToASCII(parent->children()[1]->GetTitle()));
  EXPECT_EQ("C", base::UTF16ToASCII(parent->children()[2]->GetTitle()));
  EXPECT_EQ("D", base::UTF16ToASCII(parent->children()[3]->GetTitle()));
}

TEST_F(BookmarkModelTest, NodeVisibility) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(model_->bookmark_bar_node()->IsVisible());
  EXPECT_FALSE(model_->other_node()->IsVisible());
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
#else
  EXPECT_TRUE(model_->bookmark_bar_node()->IsVisible());
  EXPECT_TRUE(model_->other_node()->IsVisible());
  EXPECT_FALSE(model_->mobile_node()->IsVisible());
#endif

  // Arbitrary node should be visible
  TestNode bbn;
  PopulateNodeFromString("B", &bbn);
  const BookmarkNode* parent = model_->mobile_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  parent = model_->other_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  // Mobile folder should be visible now that it has a child.
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
  EXPECT_TRUE(model_->other_node()->IsVisible());
}

TEST_F(BookmarkModelTest, NodeVisibility_AddBookmarkToNonVisibleFolder) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const BookmarkPermanentNode* permanent_folder = model_->other_node();
#else
  const BookmarkPermanentNode* permanent_folder = model_->mobile_node();
#endif

  // This permanent folder is not visible when empty.
  ASSERT_FALSE(permanent_folder->IsVisible());

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(permanent_folder))
      .WillOnce([](const BookmarkPermanentNode* node) {
        EXPECT_TRUE(node->IsVisible());
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer, BookmarkNodeAdded(permanent_folder, _, _))
      .WillOnce(WithArg<0>([](const BookmarkNode* parent) {
        EXPECT_TRUE(parent->IsVisible());
      }));

  // Add a bookmark to the permanent folder.
  model_->AddURL(permanent_folder, 0, u"Title", GURL("http://foo.com"));
  EXPECT_TRUE(permanent_folder->IsVisible());
}

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
TEST_F(BookmarkModelTest, NodeVisibility_AddFirstLocalBookmarkToOtherFolder) {
  model_->CreateAccountPermanentFolders();

  // To start with, only the account permanent folders are visible.
  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model_->account_bookmark_bar_node(),
                          model_->account_other_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model_->other_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model_->other_node(),
                                model_->account_bookmark_bar_node(),
                                model_->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer, BookmarkNodeAdded(model_->other_node(), _, _))
      .WillOnce(WithArg<0>([this](const BookmarkNode* parent) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model_->other_node(),
                                model_->account_bookmark_bar_node(),
                                model_->account_other_node()));
      }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(
                            model_->bookmark_bar_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(model_->bookmark_bar_node(), model_->other_node(),
                        model_->account_bookmark_bar_node(),
                        model_->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });

  // Add a bookmark to the local mobile folder.
  model_->AddURL(model_->other_node(), 0, u"Title", GURL("http://foo.com"));
  EXPECT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model_->bookmark_bar_node(), model_->other_node(),
                          model_->account_bookmark_bar_node(),
                          model_->account_other_node()));
}

TEST_F(BookmarkModelTest, NodeVisibility_AddFirstLocalBookmarkToMobileFolder) {
  model_->CreateAccountPermanentFolders();

  // To start with, only the account permanent folders are visible.
  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model_->account_bookmark_bar_node(),
                          model_->account_other_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model_->mobile_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model_->mobile_node(),
                                model_->account_bookmark_bar_node(),
                                model_->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer, BookmarkNodeAdded(model_->mobile_node(), _, _))
      .WillOnce(WithArg<0>([this](const BookmarkNode* parent) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model_->mobile_node(),
                                model_->account_bookmark_bar_node(),
                                model_->account_other_node()));
      }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(
                            model_->bookmark_bar_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(model_->bookmark_bar_node(), model_->mobile_node(),
                        model_->account_bookmark_bar_node(),
                        model_->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model_->other_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model_->bookmark_bar_node(),
                                model_->other_node(), model_->mobile_node(),
                                model_->account_bookmark_bar_node(),
                                model_->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });

  // Add a bookmark to the local mobile folder.
  model_->AddURL(model_->mobile_node(), 0, u"Title", GURL("http://foo.com"));
  EXPECT_THAT(
      GetVisiblePermanentNodes(),
      ElementsAre(model_->bookmark_bar_node(), model_->other_node(),
                  model_->mobile_node(), model_->account_bookmark_bar_node(),
                  model_->account_other_node()));
}
#endif  // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

TEST_F(BookmarkModelTest, NodeVisibility_RemoveLastBookmarkFromVisibleFolder) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const BookmarkPermanentNode* permanent_folder = model_->other_node();
#else
  const BookmarkPermanentNode* permanent_folder = model_->mobile_node();
#endif

  // This permanent folder is not visible when empty; visible when non-empty.
  ASSERT_FALSE(permanent_folder->IsVisible());
  const BookmarkNode* bookmark =
      model_->AddURL(permanent_folder, 0, u"Title", GURL("http://foo.com"));
  ASSERT_TRUE(permanent_folder->IsVisible());

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer, OnWillRemoveBookmarks(_, _, bookmark, _));
  EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, bookmark, _, _))
      .WillOnce(WithArg<2>(
          [](const BookmarkNode* node) { EXPECT_TRUE(node->IsVisible()); }));
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(permanent_folder))
      .WillOnce([](const BookmarkPermanentNode* node) {
        EXPECT_FALSE(node->IsVisible());
      });

  // Remove the bookmark.
  model_->Remove(bookmark, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);
  EXPECT_FALSE(permanent_folder->IsVisible());
}

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
TEST_F(BookmarkModelTest,
       NodeVisibility_MoveBookmarkChangesVisibilityOfSourceFolder) {
  const BookmarkPermanentNode* source_folder = model_->mobile_node();

  // This permanent folder is not visible when empty; visible when non-empty.
  ASSERT_FALSE(source_folder->IsVisible());
  ASSERT_TRUE(model_->bookmark_bar_node()->IsVisible());
  const BookmarkNode* bookmark =
      model_->AddURL(source_folder, 0, u"Title", GURL("http://foo.com"));
  ASSERT_TRUE(source_folder->IsVisible());

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer, OnWillMoveBookmarkNode(source_folder, 0,
                                               model_->bookmark_bar_node(), 0));
  EXPECT_CALL(observer, BookmarkNodeMoved(source_folder, 0,
                                          model_->bookmark_bar_node(), 0))
      .WillOnce(WithArg<0>(
          [](const BookmarkNode* node) { EXPECT_TRUE(node->IsVisible()); }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(source_folder))
      .WillOnce([](const BookmarkPermanentNode* node) {
        EXPECT_FALSE(node->IsVisible());
      });

  // Remove the bookmark.
  model_->Move(bookmark, model_->bookmark_bar_node(), 0);
  EXPECT_FALSE(source_folder->IsVisible());
}

TEST_F(BookmarkModelTest,
       NodeVisibility_MoveBookmarkChangesVisibilityOfDestinationFolder) {
  const BookmarkPermanentNode* source_folder = model_->other_node();
  const BookmarkPermanentNode* destination_folder = model_->mobile_node();

  // This source folder is visible when empty, destination folder is not.
  ASSERT_TRUE(source_folder->IsVisible());
  ASSERT_FALSE(destination_folder->IsVisible());
  const BookmarkNode* bookmark =
      model_->AddURL(source_folder, 0, u"Title", GURL("http://foo.com"));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer,
              OnWillMoveBookmarkNode(source_folder, 0, destination_folder, 0));
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(destination_folder))
      .WillOnce([](const BookmarkPermanentNode* node) {
        EXPECT_TRUE(node->IsVisible());
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer,
              BookmarkNodeMoved(source_folder, 0, destination_folder, 0));

  // Remove the bookmark.
  model_->Move(bookmark, destination_folder, 0);
  EXPECT_TRUE(destination_folder->IsVisible());
}

TEST_F(
    BookmarkModelTest,
    NodeVisibility_MoveBookmarkChangesVisibilityOfSourceAndDestinationFolder) {
  // Create empty account folders.
  model_->CreateAccountPermanentFolders();

  const BookmarkPermanentNode* source_folder = model_->account_mobile_node();
  const BookmarkPermanentNode* destination_folder = model_->mobile_node();

  // Add a node to the local bookmark bar, to simplify the test by avoiding all
  // local folders becoming invisible.
  model_->AddURL(model_->bookmark_bar_node(), 0, u"Title",
                 GURL("http://bar.com"));
  const BookmarkNode* bookmark =
      model_->AddURL(source_folder, 0, u"Title", GURL("http://foo.com"));
  ASSERT_TRUE(source_folder->IsVisible());
  ASSERT_FALSE(destination_folder->IsVisible());

  ASSERT_THAT(
      GetVisiblePermanentNodes(),
      ElementsAre(model_->bookmark_bar_node(), model_->other_node(),
                  model_->account_bookmark_bar_node(),
                  model_->account_other_node(), model_->account_mobile_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence enforce_callback_order;
  EXPECT_CALL(observer,
              OnWillMoveBookmarkNode(source_folder, 0, destination_folder, 0));
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(destination_folder))
      .WillOnce([](const BookmarkPermanentNode* destination) {
        EXPECT_TRUE(destination->IsVisible());
      });
  EXPECT_CALL(observer,
              BookmarkNodeMoved(source_folder, 0, destination_folder, 0))
      .WillOnce(WithArgs<0, 2>(
          [](const BookmarkNode* source, const BookmarkNode* destination) {
            EXPECT_TRUE(source->IsVisible());
            EXPECT_TRUE(destination->IsVisible());
          }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(source_folder))
      .WillOnce([](const BookmarkPermanentNode* source) {
        EXPECT_FALSE(source->IsVisible());
      });

  // Remove the bookmark.
  model_->Move(bookmark, destination_folder, 0);
  EXPECT_FALSE(source_folder->IsVisible());
  EXPECT_TRUE(destination_folder->IsVisible());
}

TEST_F(BookmarkModelTest, NodeVisibility_CreateAccountPermanentFolders) {
  // Check per-platform visibility of empty permanent nodes.
  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model_->bookmark_bar_node(), model_->other_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());
  testing::InSequence enforce_callback_order;

  // First expect callbacks for the previously-visible local permanent folders.
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(
                            model_->bookmark_bar_node()))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model_->other_node()));
      });
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model_->other_node()))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(), IsEmpty());
      });

  // Now expect callbacks for the newly-visible account permanent folders.
  EXPECT_CALL(observer, BookmarkNodeAdded(_, 3, _))
      .WillOnce(
          WithArgs<0, 1>([this](const BookmarkNode* parent, size_t index) {
            const BookmarkNode* node = parent->children()[index].get();
            EXPECT_EQ(node, model_->account_bookmark_bar_node());
            EXPECT_TRUE(node->IsVisible());
            EXPECT_THAT(GetVisiblePermanentNodes(),
                        ElementsAre(model_->account_bookmark_bar_node()));
          }));

  EXPECT_CALL(observer, BookmarkNodeAdded(_, 4, _))
      .WillOnce(
          WithArgs<0, 1>([this](const BookmarkNode* parent, size_t index) {
            const BookmarkNode* node = parent->children()[index].get();
            EXPECT_EQ(node, model_->account_other_node());
            EXPECT_TRUE(node->IsVisible());
            EXPECT_THAT(GetVisiblePermanentNodes(),
                        ElementsAre(model_->account_bookmark_bar_node(),
                                    model_->account_other_node()));
          }));

  EXPECT_CALL(observer, BookmarkNodeAdded(_, 5, _))
      .WillOnce(
          WithArgs<0, 1>([this](const BookmarkNode* parent, size_t index) {
            const BookmarkNode* node = parent->children()[index].get();
            EXPECT_EQ(node, model_->account_mobile_node());
            EXPECT_FALSE(node->IsVisible());
            EXPECT_THAT(GetVisiblePermanentNodes(),
                        ElementsAre(model_->account_bookmark_bar_node(),
                                    model_->account_other_node()));
          }));

  // Create empty account folders.
  model_->CreateAccountPermanentFolders();
  EXPECT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model_->account_bookmark_bar_node(),
                          model_->account_other_node()));
}

TEST_F(BookmarkModelTest, NodeVisibility_RemoveAccountPermanentFolders) {
  // Create empty account folders.
  model_->CreateAccountPermanentFolders();
  const BookmarkPermanentNode* local_bb = model_->bookmark_bar_node();
  const BookmarkPermanentNode* local_other = model_->other_node();
  const BookmarkPermanentNode* account_bb = model_->account_bookmark_bar_node();
  const BookmarkPermanentNode* account_other = model_->account_other_node();

  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(account_bb, account_other));

  // Set up observer expectations.
  testing::NiceMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model_.get());
  testing::InSequence enforce_callback_order;

  // Then expect callbacks for the previously-invisible local permanent
  // folders.
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(local_bb))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_TRUE(model_->IsLocalOnlyNode(*node));
        EXPECT_TRUE(node->IsVisible());
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(local_bb, account_bb, account_other));
      });

  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(local_other))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_TRUE(model_->IsLocalOnlyNode(*node));
        EXPECT_TRUE(node->IsVisible());
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(local_bb, local_other, account_bb, account_other));
      });

  // In the callbacks below, ensure that the sequencing is such that observing
  // code can determine what the previous visible index of the deleted nodes
  // was.
  //
  // This is checked by calling GetVisibleIndexForPermanentNode().
  EXPECT_CALL(observer,
              BookmarkNodeRemoved(_, _, model_->account_mobile_node(), _, _))
      .WillOnce(WithArg<2>([&](const BookmarkNode* node) {
        EXPECT_FALSE(model_->IsLocalOnlyNode(*node));
        EXPECT_FALSE(node->IsVisible());
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(local_bb, local_other, account_bb, account_other));
      }));

  EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, account_other, _, _))
      .WillOnce(WithArgs<1, 2>([&](size_t old_index, const BookmarkNode* node) {
        EXPECT_FALSE(model_->IsLocalOnlyNode(*node));
        EXPECT_EQ(GetVisibleIndexForPermanentNode(old_index), 3u);
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(local_bb, local_other, account_bb));
      }));

  EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, account_bb, _, _))
      .WillOnce(WithArgs<1, 2>([&](size_t old_index, const BookmarkNode* node) {
        EXPECT_FALSE(model_->IsLocalOnlyNode(*node));
        EXPECT_EQ(GetVisibleIndexForPermanentNode(old_index), 2u);
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(local_bb, local_other));
      }));

  // Remove the account folders.
  model_->RemoveAccountPermanentFolders();
  EXPECT_THAT(GetVisiblePermanentNodes(), ElementsAre(local_bb, local_other));
}
#endif  // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

TEST_F(BookmarkModelTest, NodeVisibility_AllBookmarksPhase0) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      bookmarks::kAllBookmarksBaselineFolderVisibility);
  model_->RemoveObserver(this);
  model_ = TestBookmarkClient::CreateModelWithClient(
      std::make_unique<TestBookmarkClientWithUndo>());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(model_->bookmark_bar_node()->IsVisible());
#else
  EXPECT_TRUE(model_->bookmark_bar_node()->IsVisible());
#endif

  EXPECT_TRUE(model_->other_node()->IsVisible());
  // EXPECT_FALSE(model_->mobile_node()->IsVisible());

  // Arbitrary node should be visible
  TestNode bbn;
  PopulateNodeFromString("B", &bbn);
  const BookmarkNode* parent = model_->mobile_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());
  parent = model_->other_node();
  PopulateBookmarkNode(&bbn, model_.get(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  // Mobile folder should be visible now that it has a child.
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
  EXPECT_TRUE(model_->other_node()->IsVisible());
}

TEST_F(BookmarkModelTest, MobileNodeVisibleWithChildren) {
  const BookmarkNode* mobile_node = model_->mobile_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model_->AddURL(mobile_node, 0, kTitle, kUrl);
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
  model_->AddURL(model_->other_node(), 0, u"User", GURL("http://google.com"));
  // "youtube.com" is not.
  model_->AddURL(managed_node, 0, u"Managed", GURL("http://youtube.com"));

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

  const std::u16string kTitle = u"Title";
  const BookmarkNode* user_parent = model_->other_node();
  const BookmarkNode* managed_parent = managed_node;
  const GURL kUrl("http://google.com");

  // `url` is not bookmarked yet.
  EXPECT_EQ(model_->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);

  // Having a managed node doesn't count.
  model_->AddURL(managed_parent, 0, kTitle, kUrl);
  EXPECT_EQ(model_->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);

  // Now add a user node.
  const BookmarkNode* user = model_->AddURL(user_parent, 0, kTitle, kUrl);
  EXPECT_EQ(user, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));

  // Having a more recent managed node doesn't count either.
  const BookmarkNode* managed = model_->AddURL(managed_parent, 0, kTitle, kUrl);
  EXPECT_GE(managed->date_added(), user->date_added());
  EXPECT_EQ(user, model_->GetMostRecentlyAddedUserNodeForURL(kUrl));
}

// Verifies that renaming a bookmark folder does not add the folder node to the
// autocomplete index. crbug.com/778266
TEST_F(BookmarkModelTest, RenamedFolderNodeExcludedFromIndex) {
  // Add a folder.
  const BookmarkNode* folder =
      model_->AddFolder(model_->other_node(), 0, u"MyFavorites");

  // Change the folder title.
  model_->SetTitle(folder, u"MyBookmarks", metrics::BookmarkEditSource::kOther);

  // There should be no matching bookmarks.
  std::vector<TitledUrlMatch> matches = model_->GetBookmarksMatching(
      u"MyB", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
}

TEST_F(BookmarkModelTest, GetBookmarksMatching) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const BookmarkNode* folder =
      model_->AddFolder(bookmark_bar_node, 0, u"folder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model_->AddURL(folder, 0, kTitle, kUrl);

  // Should not match incorrect paths.
  auto matches = model_->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());

  // Should match correct paths.
  matches = model_->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
}

// Verifies that TitledUrlIndex is updated when a bookmark is removed.
TEST_F(BookmarkModelTest, TitledUrlIndexUpdatedOnRemove) {
  const std::u16string kTitle = u"Title";
  const GURL kUrl("http://google.com");
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  model_->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  ASSERT_EQ(1U, model_
                    ->GetBookmarksMatching(
                        kTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());

  // Remove the node and make sure we don't get back any results.
  model_->Remove(bookmark_bar_node->children().front().get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_EQ(0U, model_
                    ->GetBookmarksMatching(
                        kTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
}

// Verifies that TitledUrlIndex is updated when a bookmark's title changes.
TEST_F(BookmarkModelTest, TitledUrlIndexUpdatedOnChangeTitle) {
  const std::u16string kInitialTitle = u"Initial";
  const std::u16string kNewTitle = u"New";
  const GURL kUrl("http://google.com");
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  model_->AddURL(bookmark_bar_node, 0, kInitialTitle, kUrl);
  ASSERT_EQ(1U,
            model_
                ->GetBookmarksMatching(kInitialTitle, 1,
                                       query_parser::MatchingAlgorithm::DEFAULT)
                .size());
  ASSERT_EQ(0U, model_
                    ->GetBookmarksMatching(
                        kNewTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());

  // Change the title.
  model_->SetTitle(bookmark_bar_node->children().front().get(), kNewTitle,
                   metrics::BookmarkEditSource::kOther);

  // Verify that we only get results for the new title.
  EXPECT_EQ(0U,
            model_
                ->GetBookmarksMatching(kInitialTitle, 1,
                                       query_parser::MatchingAlgorithm::DEFAULT)
                .size());
  EXPECT_EQ(1U, model_
                    ->GetBookmarksMatching(
                        kNewTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
}

// Verifies that TitledUrlIndex is updated when a bookmark's URL changes.
TEST_F(BookmarkModelTest, TitledUrlIndexUpdatedOnChangeURL) {
  const std::u16string kTitle = u"Title";
  const GURL kInitialUrl("http://initial");
  const GURL kNewUr("http://new");
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();

  model_->AddURL(bookmark_bar_node, 0, kTitle, kInitialUrl);
  ASSERT_EQ(1U, model_
                    ->GetBookmarksMatching(
                        u"initial", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
  ASSERT_EQ(0U, model_
                    ->GetBookmarksMatching(
                        u"new", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());

  // Change the URL.
  model_->SetURL(bookmark_bar_node->children().front().get(), kNewUr,
                 metrics::BookmarkEditSource::kOther);

  // Verify that we only get results for the new URL.
  EXPECT_EQ(0U, model_
                    ->GetBookmarksMatching(
                        u"initial", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
  EXPECT_EQ(1U, model_
                    ->GetBookmarksMatching(
                        u"new", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
}

TEST_F(BookmarkModelTest, GetNodeByUuid) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const base::Uuid kExplicitUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kExplicitUuid2 = base::Uuid::GenerateRandomV4();

  // Create two nodes (URL and folder) without specifying a UUID, which means
  // a random one is used.
  const BookmarkNode* url_node_with_implicit_uuid =
      model_->AddURL(bookmark_bar_node, 0, u"title", GURL("http://foo.com"));
  const BookmarkNode* folder_node_with_implicit_uuid =
      model_->AddFolder(bookmark_bar_node, 0, u"title");

  // Create two more with an explicit UUID provided.
  const BookmarkNode* url_node_with_explicit_uuid = model_->AddURL(
      bookmark_bar_node, 0, u"title", GURL("http://foo.com"),
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, kExplicitUuid1);
  const BookmarkNode* folder_node_with_explicit_uuid = model_->AddFolder(
      bookmark_bar_node, 0, u"title",
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, kExplicitUuid2);

  ASSERT_TRUE(url_node_with_implicit_uuid);
  ASSERT_TRUE(folder_node_with_implicit_uuid);
  ASSERT_TRUE(url_node_with_explicit_uuid);
  ASSERT_TRUE(folder_node_with_explicit_uuid);

  EXPECT_EQ(
      url_node_with_implicit_uuid,
      model_->GetNodeByUuid(url_node_with_implicit_uuid->uuid(),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      folder_node_with_implicit_uuid,
      model_->GetNodeByUuid(folder_node_with_implicit_uuid->uuid(),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(url_node_with_explicit_uuid,
            model_->GetNodeByUuid(
                kExplicitUuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder_node_with_explicit_uuid,
            model_->GetNodeByUuid(
                kExplicitUuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Verify cases that should return nullptr.
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(
                base::Uuid(), NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::GenerateRandomV4(),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetAccountNodeByUuid) {
  model_->CreateAccountPermanentFolders();
  const BookmarkNode* account_bookmark_bar_node =
      model_->account_bookmark_bar_node();
  ASSERT_NE(nullptr, account_bookmark_bar_node);
  ASSERT_EQ(account_bookmark_bar_node,
            model_->GetNodeByUuid(account_bookmark_bar_node->uuid(),
                                  NodeTypeForUuidLookup::kAccountNodes));
  ASSERT_NE(
      account_bookmark_bar_node,
      model_->GetNodeByUuid(account_bookmark_bar_node->uuid(),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Create two nodes (URL and folder) without specifying a UUID, which means
  // a random one is used.
  const BookmarkNode* url_node_with_implicit_uuid = model_->AddURL(
      account_bookmark_bar_node, 0, u"title", GURL("http://foo.com"));
  const BookmarkNode* folder_node_with_implicit_uuid =
      model_->AddFolder(account_bookmark_bar_node, 0, u"title");

  ASSERT_TRUE(url_node_with_implicit_uuid);
  ASSERT_TRUE(folder_node_with_implicit_uuid);

  EXPECT_EQ(url_node_with_implicit_uuid,
            model_->GetNodeByUuid(url_node_with_implicit_uuid->uuid(),
                                  NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder_node_with_implicit_uuid,
            model_->GetNodeByUuid(folder_node_with_implicit_uuid->uuid(),
                                  NodeTypeForUuidLookup::kAccountNodes));

  // Verify that lookups using kLocalOrSyncableNodes return nullptr.
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         url_node_with_implicit_uuid->uuid(),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         folder_node_with_implicit_uuid->uuid(),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetPermanentNodeByUuid) {
  // Permanent nodes should be returned by UUID.
  EXPECT_EQ(
      model_->root_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      model_->bookmark_bar_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(model_->mobile_node(),
            model_->GetNodeByUuid(
                base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      model_->other_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Managed bookmarks don't exist by default.
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kManagedNodeUuid),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();
  EXPECT_EQ(managed_node, model_->GetNodeByUuid(
                              base::Uuid::ParseLowercase(kManagedNodeUuid),
                              NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetAccountPermanentNodeByUuid) {
  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();
  ASSERT_NE(nullptr, managed_node);

  ASSERT_EQ(nullptr, model_->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model_->account_mobile_node());
  ASSERT_EQ(nullptr, model_->account_other_node());

  // Before account nodes are created, the lookups should return null.
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                                  NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(base::Uuid::ParseLowercase(kManagedNodeUuid),
                                  NodeTypeForUuidLookup::kAccountNodes));

  model_->CreateAccountPermanentFolders();
  ASSERT_NE(nullptr, model_->account_bookmark_bar_node());
  ASSERT_NE(nullptr, model_->account_mobile_node());
  ASSERT_NE(nullptr, model_->account_other_node());

  // The root node's UUID should still return null.
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                                  NodeTypeForUuidLookup::kAccountNodes));

  // Account permanent nodes should be returned by UUID.
  EXPECT_EQ(
      model_->account_bookmark_bar_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                            NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(model_->account_mobile_node(),
            model_->GetNodeByUuid(
                base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(
      model_->account_other_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                            NodeTypeForUuidLookup::kAccountNodes));

  // Managed bookmarks are not considered account nodes.
  EXPECT_EQ(nullptr,
            model_->GetNodeByUuid(base::Uuid::ParseLowercase(kManagedNodeUuid),
                                  NodeTypeForUuidLookup::kAccountNodes));

  // Verify that having created account bookmarks doesn't influence
  // local-or-syncable lookups.
  ASSERT_NE(model_->bookmark_bar_node(), model_->account_bookmark_bar_node());
  ASSERT_NE(model_->mobile_node(), model_->account_mobile_node());
  ASSERT_NE(model_->other_node(), model_->account_other_node());
  EXPECT_EQ(
      model_->root_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      model_->bookmark_bar_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(model_->mobile_node(),
            model_->GetNodeByUuid(
                base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      model_->other_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Removing account nodes should make lookups fail again.
  model_->RemoveAccountPermanentFolders();
  ASSERT_EQ(nullptr, model_->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model_->account_mobile_node());
  ASSERT_EQ(nullptr, model_->account_other_node());
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
}

TEST_F(BookmarkModelTest, GetNodeByUuidAfterRemove) {
  const BookmarkNode* folder1 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"title1");
  const BookmarkNode* folder2 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"title2");

  const base::Uuid uuid1 = folder1->uuid();
  const base::Uuid uuid2 = folder2->uuid();

  ASSERT_EQ(folder1, model_->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_EQ(folder2, model_->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  model_->Remove(folder1, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);

  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder2, model_->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  model_->Remove(folder2, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);

  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetNodeByUuidAfterRemoveAllUserBookmarks) {
  const BookmarkNode* folder1 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"title1");
  const BookmarkNode* folder2 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"title2");

  const base::Uuid uuid1 = folder1->uuid();
  const base::Uuid uuid2 = folder2->uuid();

  ASSERT_EQ(folder1, model_->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_EQ(folder2, model_->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  model_->RemoveAllUserBookmarks(FROM_HERE);

  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model_->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Permanent nodes should continue to exist and be returned by UUID.
  EXPECT_EQ(
      model_->bookmark_bar_node(),
      model_->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                            NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes) {
  constexpr size_t kOriginalNumberOfNodes = 4u;
  EXPECT_EQ(kOriginalNumberOfNodes,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a folder.
  const BookmarkNode* folder =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"title");
  EXPECT_EQ(kOriginalNumberOfNodes + 1,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a URL in the folder.
  model_->AddURL(folder, 0, u"title", GURL("http://foo.com"));
  EXPECT_EQ(kOriginalNumberOfNodes + 2,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Remove the bookmark URL.
  model_->RemoveLastChild(
      folder, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_EQ(kOriginalNumberOfNodes + 1,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a bookmark in the bookmark bar.
  model_->AddNewURL(model_->bookmark_bar_node(), 0, u"url",
                    GURL("http://url.com"));
  EXPECT_EQ(kOriginalNumberOfNodes + 2,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Create account folders.
  model_->CreateAccountPermanentFolders();
  EXPECT_EQ(kOriginalNumberOfNodes + 5,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Delete everything.
  model_->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_EQ(kOriginalNumberOfNodes + 3,
            model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());
}

TEST(BookmarkModelLoadTest, NodesPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  const GURL node_url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, u"User", node_url);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  base::HistogramTester histogram_tester;

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  ASSERT_EQ(1u, model->bookmark_bar_node()->children().size());
  EXPECT_EQ(node_url, model->bookmark_bar_node()->children()[0]->url());

  histogram_tester.ExpectTotalCount("Bookmarks.Storage.TimeToLoadAtStartup2",
                                    1);
}

TEST(BookmarkModelLoadTest, NodesPopulatedIncludingAccountNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with one local-or-syncable url and one account url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->CreateAccountPermanentFolders();

  ASSERT_NE(nullptr, model->account_bookmark_bar_node());

  const GURL node_url1("http://google.com/1");
  const GURL node_url2("http://google.com/2");
  model->AddURL(model->bookmark_bar_node(), 0, u"User", node_url1);
  model->AddURL(model->account_bookmark_bar_node(), 0, u"User", node_url2);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure account nodes are loaded.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  ASSERT_NE(nullptr, model->account_bookmark_bar_node());
  ASSERT_EQ(1u, model->bookmark_bar_node()->children().size());
  ASSERT_EQ(1u, model->account_bookmark_bar_node()->children().size());
  EXPECT_EQ(node_url1, model->bookmark_bar_node()->children()[0]->url());
  EXPECT_EQ(node_url2,
            model->account_bookmark_bar_node()->children()[0]->url());
}

TEST(BookmarkModelLoadTest, AccountSyncMetadataPopulatedWithoutNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Since metadata str serialized proto, it could contain non-ASCII
  // characters.
  const std::string sync_metadata_str("a/2'\"");

  // Create a model with one local-or-syncable url and no account bookmarks.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  {
    auto client = std::make_unique<TestBookmarkClient>();
    TestBookmarkClient* client_ptr = client.get();
    BookmarkModel model(std::move(client));
    model.Load(tmp_dir.GetPath());
    test::WaitForBookmarkModelToLoad(&model);

    ASSERT_EQ(nullptr, model.account_bookmark_bar_node());
    ASSERT_EQ(nullptr, model.account_other_node());
    ASSERT_EQ(nullptr, model.account_mobile_node());

    client_ptr->SetAccountBookmarkSyncMetadataAndScheduleWrite(
        sync_metadata_str);

    // This is necessary to ensure the save completes.
    task_environment.FastForwardUntilNoTasksRemain();
  }

  // Recreate the model and ensure account sync metadata is passed to
  // BookmarkClient although there are no account bookmarks.
  auto client = std::make_unique<TestBookmarkClient>();
  TestBookmarkClient* client_ptr = client.get();
  BookmarkModel model(std::move(client));
  model.Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(&model);

  EXPECT_EQ(nullptr, model.account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model.account_other_node());
  EXPECT_EQ(nullptr, model.account_mobile_node());

  EXPECT_EQ(sync_metadata_str, client_ptr->account_bookmark_sync_metadata());
}

TEST(BookmarkModelLoadTest, RemoveAccountPermanentFoldersUponMetadataDecoding) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with account bookmarks.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  {
    auto model =
        std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
    model->Load(tmp_dir.GetPath());
    test::WaitForBookmarkModelToLoad(model.get());
    model->CreateAccountPermanentFolders();

    // This is necessary to ensure the save completes.
    task_environment.FastForwardUntilNoTasksRemain();
  }

  testing::StrictMock<MockBookmarkModelObserver> observer;
  EXPECT_CALL(observer, BookmarkModelLoaded);

  // Load the model from disk, but pretend that the client responded with
  // `kMustRemoveAccountPermanentFolders` when decoding account sync metadata.
  auto client = std::make_unique<TestBookmarkClient>();
  client->SetDecodeAccountBookmarkSyncMetadataResult(
      BookmarkClient::DecodeAccountBookmarkSyncMetadataResult::
          kMustRemoveAccountPermanentFolders);
  BookmarkModel model(std::move(client));
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(&model);

  model.Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(&model);

  EXPECT_EQ(nullptr, model.account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model.account_other_node());
  EXPECT_EQ(nullptr, model.account_mobile_node());
}

// Verifies the TitledUrlIndex is properly loaded.
TEST(BookmarkModelLoadTest, TitledUrlIndexPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  const GURL node_url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, u"User", node_url);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  std::vector<TitledUrlMatch> matches = model->GetBookmarksMatching(
      u"user", 1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(node_url, matches[0].node->GetTitledUrlNodeUrl());
}

// Verifies the TitledUrlIndex is properly loaded for account bookmarks.
TEST(BookmarkModelLoadTest, TitledUrlIndexPopulatedForAccountNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->CreateAccountPermanentFolders();

  const GURL node_url("http://google.com");
  model->AddURL(model->account_bookmark_bar_node(), 0, u"User", node_url);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  std::vector<TitledUrlMatch> matches = model->GetBookmarksMatching(
      u"user", 1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(node_url, matches[0].node->GetTitledUrlNodeUrl());
}

// Verifies the UUID index is properly loaded.
TEST(BookmarkModelLoadTest, UuidIndexPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  const base::Uuid node_uuid = model
                                   ->AddURL(model->bookmark_bar_node(), 0,
                                            u"User", GURL("http://google.com"))
                                   ->uuid();

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  EXPECT_NE(nullptr,
            model->GetNodeByUuid(node_uuid,
                                 NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model->GetNodeByUuid(
                         node_uuid, NodeTypeForUuidLookup::kAccountNodes));
}

// Verifies the UUID index is properly loaded, for account nodes.
TEST(BookmarkModelLoadTest, UuidIndexPopulatedForAccountNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->CreateAccountPermanentFolders();

  const base::Uuid node_uuid =
      model
          ->AddURL(model->account_bookmark_bar_node(), 0, u"User",
                   GURL("http://google.com"))
          ->uuid();

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  EXPECT_EQ(nullptr,
            model->GetNodeByUuid(node_uuid,
                                 NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_NE(nullptr, model->GetNodeByUuid(
                         node_uuid, NodeTypeForUuidLookup::kAccountNodes));
}

TEST(BookmarkModelStorageTest,
     GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->CreateAccountPermanentFolders();

  const base::Uuid node_uuid =
      model
          ->AddURL(model->account_bookmark_bar_node(), 0, u"User",
                   GURL("http://google.com"))
          ->uuid();

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  constexpr size_t kOriginalNumberOfNodes = 7u;

  // Take into account the node loaded.
  EXPECT_EQ(kOriginalNumberOfNodes + 1,
            model->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"title");
  EXPECT_EQ(kOriginalNumberOfNodes + 2,
            model->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a URL in the folder.
  model->AddURL(folder, 0, u"title", GURL("http://foo.com"));
  EXPECT_EQ(kOriginalNumberOfNodes + 3,
            model->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Remove the bookmark URL.
  model->RemoveLastChild(folder, bookmarks::metrics::BookmarkEditSource::kOther,
                         FROM_HERE);
  EXPECT_EQ(kOriginalNumberOfNodes + 2,
            model->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a bookmark in the bookmark bar.
  model->AddNewURL(model->bookmark_bar_node(), 0, u"url",
                   GURL("http://url.com"));
  EXPECT_EQ(kOriginalNumberOfNodes + 3,
            model->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Delete everything.
  model->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_EQ(kOriginalNumberOfNodes,
            model->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());
}

TEST(BookmarkModelStorageTest, SaveExactlyOneFile) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model =
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>());
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  // Create one local-or-syncable bookmark.
  const BookmarkNode* local_or_syncable_node = model->AddURL(
      model->bookmark_bar_node(), 0, u"Foo", GURL("http://foo.com"));
  EXPECT_TRUE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  EXPECT_FALSE(model->AccountStorageHasPendingWriteForTest());

  task_environment.FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  ASSERT_FALSE(model->AccountStorageHasPendingWriteForTest());

  // Create account permanent folders.
  model->CreateAccountPermanentFolders();
  EXPECT_FALSE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  EXPECT_TRUE(model->AccountStorageHasPendingWriteForTest());

  task_environment.FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  ASSERT_FALSE(model->AccountStorageHasPendingWriteForTest());

  // Save one account bookmark.
  model->AddURL(model->account_bookmark_bar_node(), 0, u"Bar",
                GURL("http://bar.com"));
  EXPECT_FALSE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  EXPECT_TRUE(model->AccountStorageHasPendingWriteForTest());

  task_environment.FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  ASSERT_FALSE(model->AccountStorageHasPendingWriteForTest());

  // Edit the local-or-syncable bookmark.
  model->SetTitle(local_or_syncable_node, u"Foo2",
                  metrics::BookmarkEditSource::kOther);
  EXPECT_TRUE(model->LocalOrSyncableStorageHasPendingWriteForTest());
  EXPECT_FALSE(model->AccountStorageHasPendingWriteForTest());
}

TEST(BookmarkNodeTest, NodeMetaInfo) {
  BookmarkNode node(/*id=*/0, base::Uuid::GenerateRandomV4(), GURL());
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
  constexpr struct TestData {
    // Structure of the children of the bookmark model node.
    std::string_view bbn_contents;
    // Structure of the children of the other node.
    std::string_view other_contents;
    // Structure of the children of the synced node.
    std::string_view mobile_contents;
  } kTestCases[] = {
      // See PopulateNodeFromString for a description of these strings.
      {"", ""},        {"a", "b"},
      {"a [ b ]", ""}, {"", "[ b ] a [ c [ d e [ f ] ] ]"},
      {"a [ b ]", ""}, {"a b c [ d e [ f ] ]", "g h i [ j k [ l ] ]"},
  };
  std::unique_ptr<BookmarkModel> model;
  for (const auto& test_case : kTestCases) {
    model = TestBookmarkClient::CreateModel();

    TestNode bbn;
    PopulateNodeFromString(test_case.bbn_contents, &bbn);
    PopulateBookmarkNode(&bbn, model.get(), model->bookmark_bar_node());

    TestNode other;
    PopulateNodeFromString(test_case.other_contents, &other);
    PopulateBookmarkNode(&other, model.get(), model->other_node());

    TestNode mobile;
    PopulateNodeFromString(test_case.mobile_contents, &mobile);
    PopulateBookmarkNode(&mobile, model.get(), model->mobile_node());

    VerifyModelMatchesNode(&bbn, model->bookmark_bar_node());
    VerifyModelMatchesNode(&other, model->other_node());
    VerifyModelMatchesNode(&mobile, model->mobile_node());
    VerifyNoDuplicateIDs(model.get());
  }
}

TEST_F(BookmarkModelTest, CreateAccountPermanentFolders) {
  ASSERT_EQ(nullptr, model_->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model_->account_other_node());
  ASSERT_EQ(nullptr, model_->account_mobile_node());

  ClearCounts();
  model_->CreateAccountPermanentFolders();

  ASSERT_NE(nullptr, model_->account_bookmark_bar_node());
  ASSERT_NE(nullptr, model_->account_other_node());
  ASSERT_NE(nullptr, model_->account_mobile_node());

  EXPECT_TRUE(model_->account_bookmark_bar_node()->is_permanent_node());
  EXPECT_TRUE(model_->account_other_node()->is_permanent_node());
  EXPECT_TRUE(model_->account_mobile_node()->is_permanent_node());

  EXPECT_EQ(BookmarkNode::BOOKMARK_BAR,
            model_->account_bookmark_bar_node()->type());
  EXPECT_EQ(BookmarkNode::OTHER_NODE, model_->account_other_node()->type());
  EXPECT_EQ(BookmarkNode::MOBILE, model_->account_mobile_node()->type());

  AssertObserverCount(3, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

TEST_F(BookmarkModelTest, RemoveAccountPermanentFolders) {
  model_->CreateAccountPermanentFolders();

  ASSERT_NE(nullptr, model_->account_bookmark_bar_node());
  ASSERT_NE(nullptr, model_->account_other_node());
  ASSERT_NE(nullptr, model_->account_mobile_node());

  ClearCounts();
  model_->RemoveAccountPermanentFolders();

  EXPECT_EQ(nullptr, model_->account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model_->account_other_node());
  EXPECT_EQ(nullptr, model_->account_mobile_node());

  AssertObserverCount(0, 0, 3, 0, 0, 3, 0, 0, 0, 0);
}

TEST_F(BookmarkModelTest, NoOpRemoveAccountPermanentFolders) {
  ASSERT_EQ(nullptr, model_->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model_->account_other_node());
  ASSERT_EQ(nullptr, model_->account_mobile_node());

  ClearCounts();
  model_->RemoveAccountPermanentFolders();

  EXPECT_EQ(nullptr, model_->account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model_->account_other_node());
  EXPECT_EQ(nullptr, model_->account_mobile_node());

  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

TEST_F(BookmarkModelTest, IsLocalOnlyNodeWithSyncFeatureOff) {
  static_cast<TestBookmarkClient*>(model_->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(false);

  model_->CreateAccountPermanentFolders();

  ASSERT_NE(nullptr, model_->bookmark_bar_node());
  ASSERT_NE(nullptr, model_->other_node());
  ASSERT_NE(nullptr, model_->mobile_node());

  const BookmarkNode* local_folder =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"local_folder");
  const BookmarkNode* account_folder = model_->AddFolder(
      model_->account_bookmark_bar_node(), 0, u"account_folder");

  EXPECT_TRUE(model_->IsLocalOnlyNode(*model_->root_node()));
  EXPECT_TRUE(model_->IsLocalOnlyNode(*model_->bookmark_bar_node()));
  EXPECT_TRUE(model_->IsLocalOnlyNode(*model_->other_node()));
  EXPECT_TRUE(model_->IsLocalOnlyNode(*model_->mobile_node()));
  EXPECT_TRUE(model_->IsLocalOnlyNode(*local_folder));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*model_->account_bookmark_bar_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*model_->account_other_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*model_->account_mobile_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*account_folder));
}

TEST_F(BookmarkModelTest, IsLocalOnlyNodeWithSyncFeatureOn) {
  static_cast<TestBookmarkClient*>(model_->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  const BookmarkNode* folder =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"local_folder");

  EXPECT_TRUE(model_->IsLocalOnlyNode(*model_->root_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*model_->bookmark_bar_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*model_->other_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*model_->mobile_node()));
  EXPECT_FALSE(model_->IsLocalOnlyNode(*folder));
}

TEST_F(BookmarkModelTest, IsLocalOnlyNodeWithSyncFeatureOnAndDettachedNode) {
  static_cast<TestBookmarkClient*>(model_->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  auto dettached_node = std::make_unique<BookmarkNode>(
      /*id=*/200, base::Uuid::GenerateRandomV4(), GURL());

  EXPECT_TRUE(model_->IsLocalOnlyNode(*dettached_node));
}

TEST_F(BookmarkModelTest, UserFolderDepthHistograms) {
  constexpr char kUrlAddedMetricName[] = "Bookmarks.UserFolderDepth.UrlAdded";
  constexpr char kUrlOpenedMetricName[] = "Bookmarks.UserFolderDepth.UrlOpened";

  TestNode bbn;
  PopulateNodeFromString("[ ] [ [ ] [ ] ]", &bbn);
  const BookmarkNode* bookmark_bar = model_->bookmark_bar_node();
  PopulateBookmarkNode(&bbn, model_.get(), bookmark_bar);

  auto* folder_0 = bookmark_bar->children()[0].get();
  auto* folder_1 = bookmark_bar->children()[1].get();
  auto* folder_1_0 = folder_1->children()[0].get();
  auto* folder_1_1 = folder_1->children()[1].get();

  auto* url_0 =
      model_->AddNewURL(bookmark_bar, 0, u"url_0", GURL("http://url_0"));

  histogram_tester()->ExpectTotalCount(kUrlAddedMetricName, 1);
  histogram_tester()->ExpectBucketCount(kUrlAddedMetricName,
                                        /*sample=*/0, /*expected_count=*/1);

  auto* url_1 = model_->AddNewURL(folder_0, 0, u"url_1", GURL("http://url_1"));
  histogram_tester()->ExpectTotalCount(kUrlAddedMetricName, 2);
  histogram_tester()->ExpectBucketCount(kUrlAddedMetricName,
                                        /*sample=*/1, /*expected_count=*/1);

  auto* url_2 =
      model_->AddNewURL(folder_1_0, 0, u"url_2", GURL("http://url_2"));
  auto* url_3 =
      model_->AddNewURL(folder_1_1, 0, u"url_3", GURL("http://url_3"));
  model_->AddNewURL(folder_1_1, 0, u"url_4", GURL("http://url_4"));
  histogram_tester()->ExpectTotalCount(kUrlAddedMetricName, 5);
  histogram_tester()->ExpectBucketCount(kUrlAddedMetricName,
                                        /*sample=*/2, /*expected_count=*/3);

  model_->UpdateLastUsedTime(url_0, base::Time::Now(), /*just_opened=*/true);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 1);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/0, /*expected_count=*/1);

  model_->UpdateLastUsedTime(url_1, base::Time::Now(), /*just_opened=*/true);
  model_->UpdateLastUsedTime(url_1, base::Time::Now(), /*just_opened=*/true);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 3);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/1, /*expected_count=*/2);

  model_->UpdateLastUsedTime(url_2, base::Time::Now(), /*just_opened=*/true);
  model_->UpdateLastUsedTime(url_3, base::Time::Now(), /*just_opened=*/true);
  model_->UpdateLastUsedTime(url_3, base::Time::Now(), /*just_opened=*/true);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 6);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/2, /*expected_count=*/3);

  // This update isn't a result of an open, but rather a sync event.
  // The histogram count should remain the same.
  model_->UpdateLastUsedTime(url_3, base::Time::Now(), /*just_opened=*/false);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 6);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/2, /*expected_count=*/3);
}

// TODO(crbug.com/395071423): remove this test (split into smaller ones)
TEST_F(BookmarkModelTest, IsVisible) {
  // Test with empty local folders only.
  EXPECT_TRUE(model_->root_node()->IsVisible());

  // Check per-platform visibility of empty permanent nodes.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(model_->bookmark_bar_node()->IsVisible());
  EXPECT_FALSE(model_->other_node()->IsVisible());
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
#else
  EXPECT_TRUE(model_->bookmark_bar_node()->IsVisible());
  EXPECT_TRUE(model_->other_node()->IsVisible());
  EXPECT_FALSE(model_->mobile_node()->IsVisible());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Create empty account folders.
  model_->CreateAccountPermanentFolders();

  // All empty local permanent folders are now hidden.
  // The account permanent folders are visible, except for the mobile node.
  EXPECT_TRUE(model_->root_node()->IsVisible());
  EXPECT_FALSE(model_->bookmark_bar_node()->IsVisible());
  EXPECT_FALSE(model_->other_node()->IsVisible());

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
#else
  EXPECT_FALSE(model_->mobile_node()->IsVisible());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Check per-platform visibility of empty account permanent nodes.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(model_->account_bookmark_bar_node()->IsVisible());
  EXPECT_FALSE(model_->account_other_node()->IsVisible());
  EXPECT_TRUE(model_->account_mobile_node()->IsVisible());
#else
  EXPECT_TRUE(model_->account_bookmark_bar_node()->IsVisible());
  EXPECT_TRUE(model_->account_other_node()->IsVisible());
  EXPECT_FALSE(model_->account_mobile_node()->IsVisible());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Make the local bookmark bar node non-empty. Nodes that were previously
  // hidden because there were no local bookmarks are now visible.
  model_->AddURL(model_->bookmark_bar_node(), 0, u"Chromium",
                 GURL("http://www.chromium.org"));
  EXPECT_TRUE(model_->bookmark_bar_node()->IsVisible());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On mobile, the other node is always hidden when empty and the mobile node
  // is always visible (so there is no change as a result of the bookmark bar
  // becoming non-empty).
  EXPECT_FALSE(model_->other_node()->IsVisible());
  EXPECT_TRUE(model_->mobile_node()->IsVisible());
#else
  // On desktop, the other node was previously hidden and is now visible.
  EXPECT_TRUE(model_->other_node()->IsVisible());
#endif

  // Make the account mobile node folder non-empty. It is now visible.
  model_->AddURL(model_->account_mobile_node(), 0, u"Chromium",
                 GURL("http://www.chromium.org"));
  EXPECT_TRUE(model_->account_mobile_node()->IsVisible());
}

}  // namespace

class BookmarkModelFaviconTest : public testing::Test,
                                 public BookmarkModelObserver {
 public:
  BookmarkModelFaviconTest()
      : model_(TestBookmarkClient::CreateModelWithClient(
            std::make_unique<TestBookmarkClientWithUndo>())) {
    model_->AddObserver(this);
  }
  ~BookmarkModelFaviconTest() override { model_->RemoveObserver(this); }

  BookmarkModelFaviconTest(const BookmarkModelFaviconTest&) = delete;
  BookmarkModelFaviconTest& operator=(const BookmarkModelFaviconTest&) = delete;

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
    model_->OnFaviconDataAvailable(node, image_result);
  }

  bool WasNodeUpdated(const BookmarkNode* node) {
    return base::Contains(updated_nodes_, node);
  }

  void ClearUpdatedNodes() { updated_nodes_.clear(); }

 protected:
  void BookmarkModelLoaded(bool ids_reassigned) override {}

  void BookmarkNodeMoved(const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {}

  void BookmarkNodeAdded(const BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}

  void BookmarkNodeRemoved(const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override {}

  void BookmarkNodeChanged(const BookmarkNode* node) override {}

  void BookmarkNodeFaviconChanged(const BookmarkNode* node) override {
    updated_nodes_.push_back(node);
  }

  void BookmarkNodeChildrenReordered(const BookmarkNode* node) override {}

  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override {}

  std::unique_ptr<BookmarkModel> model_;
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> updated_nodes_;
};

// Test that BookmarkModel::OnFaviconsChanged() sends a notification that the
// favicon changed to each BookmarkNode which has either a matching page URL
// (e.g. http://www.google.com) or a matching icon URL
// (e.g. http://www.google.com/favicon.ico).
TEST_F(BookmarkModelFaviconTest, FaviconsChangedObserver) {
  const BookmarkNode* bookmark_bar_node = model_->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kPageURL1("http://www.google.com");
  const GURL kPageURL2("http://www.google.ca");
  const GURL kPageURL3("http://www.amazon.com");
  const GURL kFaviconURL12("http://www.google.com/favicon.ico");
  const GURL kFaviconURL3("http://www.amazon.com/favicon.ico");

  const BookmarkNode* node1 =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kPageURL1);
  const BookmarkNode* node2 =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kPageURL2);
  const BookmarkNode* node3 =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kPageURL3);
  const BookmarkNode* node4 =
      model_->AddURL(bookmark_bar_node, 0, kTitle, kPageURL3);

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

TEST_F(BookmarkModelFaviconTest, ShouldResetFaviconStatusAfterRestore) {
  const std::u16string kTitle(u"foo");
  const GURL kPageURL("http://www.google.com");

  const BookmarkNode* bookmark_bar = model_->bookmark_bar_node();
  const BookmarkNode* node = model_->AddURL(bookmark_bar, 0, kTitle, kPageURL);

  ASSERT_FALSE(node->is_favicon_loaded());
  ASSERT_FALSE(node->is_favicon_loading());

  // Initiate favicon loading.
  model_->GetFavicon(node);
  ASSERT_TRUE(node->is_favicon_loading());

  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                 FROM_HERE);

  ASSERT_TRUE(static_cast<TestBookmarkClientWithUndo*>(model_->client())
                  ->RestoreLastRemovedBookmark(model_.get()));

  EXPECT_FALSE(node->is_favicon_loading());
  EXPECT_FALSE(node->is_favicon_loaded());
}

class BookmarkModelPeriodicLoggingTest : public testing::Test {
 public:
  BookmarkModelPeriodicLoggingTest() {
    auto client = std::make_unique<TestBookmarkClient>();
    test_client_ = client.get();
    model_ = TestBookmarkClient::CreateModelWithClient(std::move(client));
  }

  BookmarkModel* model() { return model_.get(); }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  // Triggers the metrics callback in the testing client - simulates that the
  // logging interval has reached.
  void SimulatePersistentLogIntervalTriggered() {
    test_client_->TriggerPersistentLogInterval();
  }

 private:
  base::HistogramTester histogram_tester_;

  std::unique_ptr<BookmarkModel> model_;
  raw_ptr<TestBookmarkClient> test_client_;

  base::test::ScopedFeatureList features_{
      switches::kSyncEnableBookmarksInTransportMode};
};

TEST_F(BookmarkModelPeriodicLoggingTest, LogOnlyIfAccountNodes) {
  model()->AddURL(model()->bookmark_bar_node(), 0, u"title",
                  GURL("http://foo.com"));

  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 0);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 0);

  model()->CreateAccountPermanentFolders();
  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 1);
}

TEST_F(BookmarkModelPeriodicLoggingTest, LogOnlyIfHaveBookmarks) {
  model()->CreateAccountPermanentFolders();

  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 0);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 0);

  model()->AddURL(model()->bookmark_bar_node(), 0, u"title",
                  GURL("http://foo.com"));
  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 1);
}

TEST_F(BookmarkModelPeriodicLoggingTest, LogAllBookmarks) {
  model()->CreateAccountPermanentFolders();
  const BookmarkNode* local_node = model()->AddURL(
      model()->other_node(), 0, u"local", GURL("http://foo.com"));

  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectUniqueSample(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalOnly, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 0);

  // Add an account bookmark in a different permanent folder.
  model()->AddURL(model()->account_mobile_node(), 0, u"account",
                  GURL("http://foo.com"));
  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalOnly, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalAndAccount, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 2);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 0);

  model()->Remove(local_node, metrics::BookmarkEditSource::kOther, FROM_HERE);
  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalOnly, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalAndAccount, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 3);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 0);
}

TEST_F(BookmarkModelPeriodicLoggingTest, LogBookmarksBarAndAllBookmarks) {
  model()->CreateAccountPermanentFolders();
  const BookmarkNode* account_node =
      model()->AddURL(model()->account_bookmark_bar_node(), 0, u"account",
                      GURL("http://foo.com"));

  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectUniqueSample(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);
  histogram_tester()->ExpectUniqueSample(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);

  // Add a local bookmark in the bookmark bar node.
  model()->AddURL(model()->bookmark_bar_node(), 0, u"local",
                  GURL("http://foo.com"));
  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar",
      metrics::BookmarksExistInStorageType::kLocalAndAccount, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 2);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalAndAccount, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 2);

  model()->Remove(account_node, metrics::BookmarkEditSource::kOther, FROM_HERE);
  SimulatePersistentLogIntervalTriggered();

  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar",
      metrics::BookmarksExistInStorageType::kLocalOnly, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar",
      metrics::BookmarksExistInStorageType::kLocalAndAccount, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.UnderBookmarksBar", 3);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalOnly, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kLocalAndAccount, 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks",
      metrics::BookmarksExistInStorageType::kAccountOnly, 1);
  histogram_tester()->ExpectTotalCount(
      "Bookmarks.BookmarksExistInStorageType.ConsideringAllBookmarks", 3);
}

}  // namespace bookmarks
