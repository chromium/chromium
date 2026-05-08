// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/bookmark_test_with_encryption_stages.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/query_parser/query_parser.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
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

// TestBookmarkClient that has harded-coded sync tokens.
class TestBookmarkClientWithFixedSyncMetadata : public TestBookmarkClient {
 public:
  TestBookmarkClientWithFixedSyncMetadata(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      std::string local_or_syncable_bookmark_sync_metadata,
      std::string account_bookmark_sync_metadata)
      : TestBookmarkClient(os_crypt_async),
        local_or_syncable_bookmark_sync_metadata_(
            local_or_syncable_bookmark_sync_metadata),
        account_bookmark_sync_metadata_(account_bookmark_sync_metadata) {}
  ~TestBookmarkClientWithFixedSyncMetadata() override = default;

  std::string EncodeLocalOrSyncableBookmarkSyncMetadata() override {
    return local_or_syncable_bookmark_sync_metadata_;
  }

  std::string EncodeAccountBookmarkSyncMetadata() override {
    return account_bookmark_sync_metadata_;
  }

 private:
  std::string local_or_syncable_bookmark_sync_metadata_;
  std::string account_bookmark_sync_metadata_;
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
      ASSERT_EQ(BookmarkNode::FOLDER, actual_child->type());
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

class BookmarkModelTest : public testing::Test {
 public:
  BookmarkModelTest()
      : model_(TestBookmarkClient::CreateModelWithClient(
            std::make_unique<TestBookmarkClientWithUndo>())) {
    model_->AddObserver(&mock_observer_);
  }

  ~BookmarkModelTest() override { model_->RemoveObserver(&mock_observer_); }

  BookmarkModelTest(const BookmarkModelTest&) = delete;
  BookmarkModelTest& operator=(const BookmarkModelTest&) = delete;

  BookmarkPermanentNode* ReloadModelWithManagedNode() {
    auto client = std::make_unique<TestBookmarkClient>();
    BookmarkPermanentNode* managed_node = client->EnableManagedNode();
    ResetModelWithClient(std::move(client));

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
  MockBookmarkModelObserver& mock_observer() { return mock_observer_; }
  base::UserActionTester* user_action_tester() { return &user_action_tester_; }

  BookmarkModel* model() { return model_.get(); }

  void ResetModelWithClient(std::unique_ptr<TestBookmarkClient> client) {
    model_->RemoveObserver(&mock_observer_);
    model_ = TestBookmarkClient::CreateModelWithClient(std::move(client));
    model_->AddObserver(&mock_observer_);
  }

 private:
  base::test::ScopedFeatureList features_{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<BookmarkModel> model_;
  base::HistogramTester histogram_tester_;
  testing::NiceMock<MockBookmarkModelObserver> mock_observer_;
  base::UserActionTester user_action_tester_;
};

TEST_F(BookmarkModelTest, InitialState) {
  const BookmarkNode* bb_node = model()->bookmark_bar_node();
  ASSERT_NE(bb_node, nullptr);
  EXPECT_EQ(0u, bb_node->children().size());
  EXPECT_EQ(BookmarkNode::BOOKMARK_BAR, bb_node->type());

  const BookmarkNode* other_node = model()->other_node();
  ASSERT_NE(other_node, nullptr);
  EXPECT_EQ(0u, other_node->children().size());
  EXPECT_EQ(BookmarkNode::OTHER_NODE, other_node->type());

  const BookmarkNode* mobile_node = model()->mobile_node();
  ASSERT_NE(mobile_node, nullptr);
  EXPECT_EQ(0u, mobile_node->children().size());
  EXPECT_EQ(BookmarkNode::MOBILE, mobile_node->type());

  EXPECT_NE(bb_node->id(), other_node->id());
  EXPECT_NE(bb_node->id(), mobile_node->id());
  EXPECT_NE(other_node->id(), mobile_node->id());
}

TEST_F(BookmarkModelTest, AddURL) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(bookmark_bar_node, 0, false));

  const BookmarkNode* new_node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model()->bookmark_bar_node()->id()),
                             testing::Ne(model()->other_node()->id()),
                             testing::Ne(model()->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddNewURL) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(bookmark_bar_node, 0, true));

  const BookmarkNode* new_node =
      model()->AddNewURL(bookmark_bar_node, 0, kTitle, kUrl);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model()->bookmark_bar_node()->id()),
                             testing::Ne(model()->other_node()->id()),
                             testing::Ne(model()->mobile_node()->id())));
}

// Tests recording user action when adding a bookmark in account storage when
// a shared BookmarkModel instance is used for account bookmarks and
// local-or-syncable ones.
TEST_F(BookmarkModelTest,
       AddNewURLAccountStorageOnSharedBookmarkModelInstance) {
  model()->CreateAccountPermanentFolders();
  model()->AddNewURL(model()->account_bookmark_bar_node(), 0, u"title",
                     GURL("http://foo.com"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.Added"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.Added.AccountStorage"));
}

// Tests recording user action when adding a bookmark in local storage not
// syncing.
TEST_F(BookmarkModelTest, AddNewURLLocalStorageNotSyncing) {
  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"title",
                     GURL("http://foo.com"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.Added"));
  EXPECT_EQ(
      1, user_action_tester()->GetActionCount("Bookmarks.Added.LocalStorage"));
}

// Tests recording user action when adding a bookmark in local storage syncing.
TEST_F(BookmarkModelTest, AddNewURLLocalStorageSyncing) {
  static_cast<TestBookmarkClient*>(model()->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"title",
                     GURL("http://foo.com"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.Added"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.Added.LocalStorageSyncing"));
}

// Tests recording user action when adding a folder in account storage.
TEST_F(BookmarkModelTest, AddNewFolderAccountStorage) {
  model()->CreateAccountPermanentFolders();

  model()->AddFolder(model()->account_mobile_node(), 0, u"title");
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.FolderAdded"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.FolderAdded.AccountStorage"));
}

// Tests recording user action when adding a folder in local storage not
// syncing.
TEST_F(BookmarkModelTest, AddNewFolderLocalStorageNotSyncing) {
  model()->AddFolder(model()->mobile_node(), 0, u"title");
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.FolderAdded"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.FolderAdded.LocalStorage"));
}

// Tests recording user action when adding a folder in local storage syncing.
TEST_F(BookmarkModelTest, AddNewFolderLocalStorageSyncing) {
  static_cast<TestBookmarkClient*>(model()->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  model()->AddFolder(model()->mobile_node(), 0, u"title");
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Bookmarks.FolderAdded"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Bookmarks.FolderAdded.LocalStorageSyncing"));
}

TEST_F(BookmarkModelTest, AddURLWithUnicodeTitle) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(
      u"\u767e\u5ea6\u4e00\u4e0b\uff0c\u4f60\u5c31\u77e5\u9053");
  const GURL kUrl("https://www.baidu.com/");

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(bookmark_bar_node, 0, false));

  const BookmarkNode* new_node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model()->bookmark_bar_node()->id()),
                             testing::Ne(model()->other_node()->id()),
                             testing::Ne(model()->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddURLWithWhitespaceTitle) {
  for (size_t i = 0; i < std::size(kUrlWhitespaceTestCases); ++i) {
    const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
    const std::u16string kTitle(
        ASCIIToUTF16(kUrlWhitespaceTestCases[i].input_title));
    const GURL kUrl("http://foo.com");

    const BookmarkNode* new_node =
        model()->AddURL(bookmark_bar_node, i, kTitle, kUrl);

    EXPECT_EQ(i + 1, bookmark_bar_node->children().size());
    EXPECT_EQ(ASCIIToUTF16(kUrlWhitespaceTestCases[i].expected_title),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::URL, new_node->type());
  }
}

TEST_F(BookmarkModelTest, AddURLWithCreationTimeAndMetaInfo) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const Time kTime = Time::Now() - base::Days(1);
  BookmarkNode::MetaInfoMap meta_info;
  meta_info["foo"] = "bar";

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(bookmark_bar_node, 0, false));

  const BookmarkNode* new_node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl, &meta_info, kTime);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(kTime, new_node->date_added());
  ASSERT_TRUE(new_node->GetMetaInfoMap());
  ASSERT_EQ(meta_info, *new_node->GetMetaInfoMap());
  ASSERT_EQ(new_node, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model()->bookmark_bar_node()->id()),
                             testing::Ne(model()->other_node()->id()),
                             testing::Ne(model()->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddURLWithUUID) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const Time kTime = Time::Now() - base::Days(1);
  BookmarkNode::MetaInfoMap meta_info;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  const BookmarkNode* new_node = model()->AddURL(
      bookmark_bar_node, /*index=*/0, kTitle, kUrl, &meta_info, kTime, kUuid);

  EXPECT_EQ(kUuid, new_node->uuid());
}

TEST_F(BookmarkModelTest, AddURLToMobileBookmarks) {
  const BookmarkPermanentNode* mobile_node = model()->mobile_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(mobile_node, 0, false));

  const BookmarkNode* new_node = model()->AddURL(mobile_node, 0, kTitle, kUrl);

  ASSERT_EQ(1u, mobile_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_EQ(kUrl, new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_EQ(new_node, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model()->bookmark_bar_node()->id()),
                             testing::Ne(model()->other_node()->id()),
                             testing::Ne(model()->mobile_node()->id())));
}

TEST_F(BookmarkModelTest, AddFolder) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(bookmark_bar_node, 0, false));

  const BookmarkNode* new_node =
      model()->AddFolder(bookmark_bar_node, 0, kTitle);

  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(kTitle, new_node->GetTitle());
  ASSERT_TRUE(new_node->uuid().is_valid());
  ASSERT_EQ(BookmarkNode::FOLDER, new_node->type());

  EXPECT_THAT(new_node->id(),
              testing::AllOf(testing::Ne(model()->bookmark_bar_node()->id()),
                             testing::Ne(model()->other_node()->id()),
                             testing::Ne(model()->mobile_node()->id())));

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  // Add another folder, just to make sure folder_ids are incremented correctly.
  EXPECT_CALL(mock_observer(), BookmarkNodeAdded(bookmark_bar_node, 0, false));
  model()->AddFolder(bookmark_bar_node, 0, kTitle);
}

TEST_F(BookmarkModelTest, AddFolderWithCreationTime) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  BookmarkNode::MetaInfoMap meta_info;
  const base::Time kCreationTime(base::Time::Now() - base::Days(1));

  const BookmarkNode* new_node = model()->AddFolder(
      bookmark_bar_node, /*index=*/0, kTitle, &meta_info, kCreationTime);

  EXPECT_EQ(kCreationTime, new_node->date_added());
}

TEST_F(BookmarkModelTest, AddFolderWithUUID) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  BookmarkNode::MetaInfoMap meta_info;
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  const BookmarkNode* new_node =
      model()->AddFolder(bookmark_bar_node, /*index=*/0, kTitle, &meta_info,
                         /*creation_time=*/Time::Now(), kUuid);

  EXPECT_EQ(kUuid, new_node->uuid());
}

TEST_F(BookmarkModelTest, AddFolderWithWhitespaceTitle) {
  for (size_t i = 0; i < std::size(kTitleWhitespaceTestCases); ++i) {
    const auto& test_case = kTitleWhitespaceTestCases[i];
    const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
    const std::u16string kTitle(ASCIIToUTF16(test_case.input_title));

    const BookmarkNode* new_node =
        model()->AddFolder(bookmark_bar_node, i, kTitle);

    EXPECT_EQ(i + 1, bookmark_bar_node->children().size());
    EXPECT_EQ(ASCIIToUTF16(test_case.expected_title), new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  }
}

TEST_F(BookmarkModelTest, RemoveURL) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const base::Location kLocation = FROM_HERE;

  const BookmarkNode* node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(),
              OnWillRemoveBookmarks(bookmark_bar_node, 0, node, kLocation));
  EXPECT_CALL(mock_observer(),
              BookmarkNodeRemoved(bookmark_bar_node, 0, node, _, kLocation));

  model()->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                  kLocation);
  ASSERT_EQ(0u, bookmark_bar_node->children().size());
  histogram_tester()->ExpectTotalCount("Bookmarks.RemovedSource", 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.RemovedSource",
      static_cast<int>(metrics::BookmarkEditSource::kOther), 1);

  // Make sure there is no mapping for the URL.
  ASSERT_EQ(model()->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);
}

TEST_F(BookmarkModelTest, RemoveFolder) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(bookmark_bar_node, 0, u"foo");

  // Add a URL as a child.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  model()->AddURL(folder, 0, kTitle, kUrl);

  const base::Location kLocation = FROM_HERE;

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(),
              OnWillRemoveBookmarks(bookmark_bar_node, 0, folder, kLocation));
  EXPECT_CALL(mock_observer(),
              BookmarkNodeRemoved(bookmark_bar_node, 0, folder, _, kLocation));

  // Now remove the folder.
  model()->Remove(folder, bookmarks::metrics::BookmarkEditSource::kOther,
                  kLocation);
  ASSERT_EQ(0u, bookmark_bar_node->children().size());
  histogram_tester()->ExpectTotalCount("Bookmarks.RemovedSource", 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.RemovedSource",
      static_cast<int>(metrics::BookmarkEditSource::kOther), 1);

  // Make sure there is no mapping for the URL.
  ASSERT_EQ(model()->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);
}

TEST_F(BookmarkModelTest, RemoveLastChild) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle1(u"foo1");
  const std::u16string kTitle2(u"foo2");
  const GURL kUrl1("http://foo1.com");
  const GURL kUrl2("http://foo2.com");
  const base::Location kLocation = FROM_HERE;

  model()->AddURL(bookmark_bar_node, 0, kTitle1, kUrl1);

  const BookmarkNode* node2 =
      model()->AddURL(bookmark_bar_node, 1, kTitle2, kUrl2);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(),
              OnWillRemoveBookmarks(bookmark_bar_node, 1, node2, kLocation));
  EXPECT_CALL(mock_observer(),
              BookmarkNodeRemoved(bookmark_bar_node, 1, node2, _, kLocation));

  model()->RemoveLastChild(bookmark_bar_node,
                           bookmarks::metrics::BookmarkEditSource::kOther,
                           kLocation);
  EXPECT_EQ(1u, bookmark_bar_node->children().size());
  histogram_tester()->ExpectTotalCount("Bookmarks.RemovedSource", 1);
  histogram_tester()->ExpectBucketCount(
      "Bookmarks.RemovedSource",
      static_cast<int>(metrics::BookmarkEditSource::kOther), 1);

  ASSERT_NE(nullptr, model()->GetMostRecentlyAddedUserNodeForURL(kUrl1));
  // Make sure there is no mapping for the removed URL.
  EXPECT_EQ(nullptr, model()->GetMostRecentlyAddedUserNodeForURL(kUrl2));
}

TEST_F(BookmarkModelTest, RemoveAllUserBookmarks) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  // Add a folder with child URL.
  const BookmarkNode* folder = model()->AddFolder(bookmark_bar_node, 0, kTitle);
  model()->AddURL(folder, 0, kTitle, kUrl);

  const base::Location kLocation = FROM_HERE;

  size_t permanent_node_count = model()->root_node()->children().size();

  EXPECT_CALL(mock_observer(), BookmarkNodeRemoved).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillRemoveAllUserBookmarks(kLocation));
  EXPECT_CALL(mock_observer(), ExtensiveBookmarkChangesBeginning());
  EXPECT_CALL(mock_observer(), ExtensiveBookmarkChangesEnded());
  EXPECT_CALL(mock_observer(),
              BookmarkAllUserNodesRemoved(
                  testing::ContainerEq(std::set<GURL>{kUrl}), kLocation));
  EXPECT_CALL(mock_observer(), GroupedBookmarkChangesBeginning());
  EXPECT_CALL(mock_observer(), GroupedBookmarkChangesEnded());

  model()->RemoveAllUserBookmarks(kLocation);

  EXPECT_EQ(0u, bookmark_bar_node->children().size());
  // No permanent node should be removed.
  EXPECT_EQ(permanent_node_count, model()->root_node()->children().size());
}

TEST_F(BookmarkModelTest, UpdateLastUsedTimeInRange) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  const base::Time kAddedTime = base::Time::Now();
  const base::Time kUsedTime1 = kAddedTime + base::Days(2);

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* url_node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl, nullptr, kAddedTime);
  model()->UpdateLastUsedTime(url_node, kUsedTime1, /*just_opened=*/true);
  EXPECT_EQ(kUsedTime1, url_node->date_last_used());
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceLastUsed", 0);
  histogram_tester()->ExpectTotalCount("Bookmarks.Opened.TimeSinceAdded", 1);
  histogram_tester()->ExpectBucketCount("Bookmarks.Opened.TimeSinceAdded", 2,
                                        1);

  const base::Time kUsedTime2 = kAddedTime + base::Days(7);
  model()->UpdateLastUsedTime(url_node, kUsedTime2, /*just_opened=*/true);
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
  model()->UpdateLastUsedTime(url_node, kUsedTime3, /*just_opened=*/false);
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
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  const base::Time kTime = base::Time::Now();

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* url_node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  model()->UpdateLastUsedTime(url_node, kTime, /*just_opened=*/true);

  // Add a folder with child URL.
  const BookmarkNode* folder = model()->AddFolder(bookmark_bar_node, 0, kTitle);
  const BookmarkNode* folder_url_node =
      model()->AddURL(folder, 0, kTitle, kUrl);
  model()->UpdateLastUsedTime(folder_url_node, kTime, /*just_opened=*/true);
  EXPECT_EQ(kTime, url_node->date_last_used());
  EXPECT_EQ(kTime, folder_url_node->date_last_used());

  model()->ClearLastUsedTimeInRange(kTime - base::Seconds(1),
                                    kTime + base::Seconds(1));
  EXPECT_EQ(base::Time(), url_node->date_last_used());
  EXPECT_EQ(base::Time(), folder_url_node->date_last_used());
}

TEST_F(BookmarkModelTest, ClearLastUsedTimeInRangeForAllTime) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  const base::Time kTime = base::Time::Now();

  // Add a url to bookmark bar.
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* url_node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  model()->UpdateLastUsedTime(url_node, kTime, /*just_opened=*/true);

  // Add a folder with child URL.
  const BookmarkNode* folder = model()->AddFolder(bookmark_bar_node, 0, kTitle);
  const BookmarkNode* folder_url_node =
      model()->AddURL(folder, 0, kTitle, kUrl);
  model()->UpdateLastUsedTime(folder_url_node, kTime, /*just_opened=*/true);
  EXPECT_EQ(kTime, url_node->date_last_used());
  EXPECT_EQ(kTime, folder_url_node->date_last_used());

  model()->ClearLastUsedTimeInRange(base::Time(), base::Time::Max());
  EXPECT_EQ(base::Time(), url_node->date_last_used());
  EXPECT_EQ(base::Time(), folder_url_node->date_last_used());
}

TEST_F(BookmarkModelTest, SetTitle) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kOriginalTitle(u"foo");
  const GURL kUrl("http://url.com");
  const BookmarkNode* node =
      model()->AddURL(bookmark_bar_node, 0, kOriginalTitle, kUrl);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillChangeBookmarkNode(node));
  EXPECT_CALL(mock_observer(), BookmarkNodeChanged(node));

  const std::u16string kNewTitle(u"goo");
  model()->SetTitle(node, kNewTitle, metrics::BookmarkEditSource::kOther);
  EXPECT_EQ(kNewTitle, node->GetTitle());

  // Should update the index.
  auto matches =
      model()->GetBookmarksMatching(kOriginalTitle, /*max_count=*/1,
                                    query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model()->GetBookmarksMatching(
      kNewTitle, /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(kUrl, matches[0].node->GetTitledUrlNodeUrl());
  histogram_tester()->ExpectBucketCount("Bookmarks.EditTitleSource",
                                        metrics::BookmarkEditSource::kOther, 1);
}

TEST_F(BookmarkModelTest, SetTitleWithWhitespace) {
  for (const auto& test_case : kTitleWhitespaceTestCases) {
    const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
    const GURL kUrl("http://foo.com");
    const BookmarkNode* node =
        model()->AddURL(bookmark_bar_node, 0, u"dummy", kUrl);

    const std::u16string title = ASCIIToUTF16(test_case.input_title);
    model()->SetTitle(node, title, metrics::BookmarkEditSource::kOther);
    EXPECT_EQ(ASCIIToUTF16(test_case.expected_title), node->GetTitle());
  }
}

TEST_F(BookmarkModelTest, SetFolderTitle) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* folder =
      model()->AddFolder(bookmark_bar_node, 0, u"folder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model()->AddURL(folder, 0, kTitle, kUrl);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillChangeBookmarkNode(folder));
  EXPECT_CALL(mock_observer(), BookmarkNodeChanged(folder));

  model()->SetTitle(folder, u"golder", metrics::BookmarkEditSource::kOther);

  // Should not change the hierarchy.
  EXPECT_EQ(bookmark_bar_node->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children().front().get(), folder);
  EXPECT_EQ(folder->children().size(), 1u);
  EXPECT_EQ(folder->children().front().get(), node);
  EXPECT_EQ(node->parent(), folder);

  // Should update the index.
  auto matches = model()->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model()->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].node, node);
  EXPECT_EQ(matches[0].node->GetTitledUrlNodeUrl(), kUrl);
  histogram_tester()->ExpectBucketCount("Bookmarks.EditTitleSource",
                                        metrics::BookmarkEditSource::kOther, 1);
}

TEST_F(BookmarkModelTest, SetURL) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kOriginalUrl("http://foo.com");
  const BookmarkNode* node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kOriginalUrl);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillChangeBookmarkNode(node));
  EXPECT_CALL(mock_observer(), BookmarkNodeChanged(node));

  const GURL kNewUrl("http://foo2.com");
  model()->SetURL(node, kNewUrl, metrics::BookmarkEditSource::kOther);
  EXPECT_EQ(kNewUrl, node->url());
  histogram_tester()->ExpectBucketCount("Bookmarks.EditURLSource",
                                        metrics::BookmarkEditSource::kOther, 1);
}

TEST_F(BookmarkModelTest, SetDateAdded) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);

  const base::Time kNewTime = base::Time::Now() + base::Minutes(20);
  EXPECT_CALL(mock_observer(), BookmarkNodeAdded).Times(0);
  EXPECT_CALL(mock_observer(), BookmarkNodeChanged).Times(0);
  model()->SetDateAdded(node, kNewTime);
  EXPECT_EQ(kNewTime, node->date_added());
  EXPECT_EQ(kNewTime, model()->bookmark_bar_node()->date_folder_modified());
}

TEST_F(BookmarkModelTest, Move) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  const BookmarkNode* folder1 =
      model()->AddFolder(bookmark_bar_node, 0, u"folder");

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer(),
                OnWillMoveBookmarkNode(bookmark_bar_node, 1, folder1, 0));
    EXPECT_CALL(mock_observer(),
                BookmarkNodeMoved(bookmark_bar_node, 1, folder1, 0));
  }

  model()->Move(node, folder1, 0);

  EXPECT_EQ(folder1, node->parent());
  EXPECT_EQ(1u, bookmark_bar_node->children().size());
  EXPECT_EQ(folder1, bookmark_bar_node->children().front().get());
  EXPECT_EQ(1u, folder1->children().size());
  EXPECT_EQ(node, folder1->children().front().get());

  auto matches = model()->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  // And remove the folder.
  const base::Location kLocation = FROM_HERE;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer(), OnWillRemoveBookmarks(bookmark_bar_node, 0,
                                                       folder1, kLocation));
    EXPECT_CALL(mock_observer(), BookmarkNodeRemoved(bookmark_bar_node, 0,
                                                     folder1, _, kLocation));
  }
  model()->Remove(folder1, bookmarks::metrics::BookmarkEditSource::kOther,
                  kLocation);
  EXPECT_EQ(model()->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);
  EXPECT_EQ(0u, bookmark_bar_node->children().size());

  matches = model()->GetBookmarksMatching(
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
  BookmarkNode* parent = AsMutable(model()->bookmark_bar_node());
  PopulateBookmarkNode(&abc, model(), parent);

  // Move to current_index - 1 moves to the left: [a, b, c] -> [b, a, c]
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer(), OnWillMoveBookmarkNode(parent, 1, parent, 0));
    EXPECT_CALL(mock_observer(), BookmarkNodeMoved(parent, 1, parent, 0));

    model()->Move(parent->children()[1].get(), parent, 0);
  }
  VerifyModelMatchesNode(&bac, parent);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  // Move the current_index is a no-op: [b, a, c] -> [b, a, c]
  {
    EXPECT_CALL(mock_observer(), OnWillMoveBookmarkNode).Times(0);
    EXPECT_CALL(mock_observer(), BookmarkNodeMoved).Times(0);
    model()->Move(parent->children()[1].get(), parent, 1);
  }
  VerifyModelMatchesNode(&bac, parent);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  // Move to current_index + 1 is a no-op: [b, a, c] -> [b, a, c]
  {
    EXPECT_CALL(mock_observer(), OnWillMoveBookmarkNode).Times(0);
    EXPECT_CALL(mock_observer(), BookmarkNodeMoved).Times(0);
    model()->Move(parent->children()[1].get(), parent, 2);
  }
  VerifyModelMatchesNode(&bac, parent);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  // Move to current_index + 2 moves 1 to the right: [b, a, c] -> [b, c, a]
  // Note: this tests moving to an index that is one past the end of the list.
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer(), OnWillMoveBookmarkNode(parent, 1, parent, 2));
    EXPECT_CALL(mock_observer(), BookmarkNodeMoved(parent, 1, parent, 2));

    model()->Move(parent->children()[1].get(), parent, 3);
  }
  VerifyModelMatchesNode(&bca, parent);
}

TEST_F(BookmarkModelTest, NonMovingMoveCall) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const base::Time kOldDate(base::Time::Now() - base::Days(1));

  const BookmarkNode* node =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  model()->SetDateFolderModified(bookmark_bar_node, kOldDate);

  // Since `node` is already at the index 0 of `bookmark_bar_node`, this is
  // no-op.
  model()->Move(node, bookmark_bar_node, 0);

  // Check that the modification date is kept untouched.
  EXPECT_EQ(kOldDate, bookmark_bar_node->date_folder_modified());
}

TEST_F(BookmarkModelTest, MoveURLFromFolder) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* folder1 =
      model()->AddFolder(bookmark_bar_node, 0, u"folder");
  const BookmarkNode* folder2 =
      model()->AddFolder(bookmark_bar_node, 0, u"golder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model()->AddURL(folder1, 0, kTitle, kUrl);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer(),
                OnWillMoveBookmarkNode(folder1, 0, folder2, 0));
    EXPECT_CALL(mock_observer(), BookmarkNodeMoved(folder1, 0, folder2, 0));
  }

  model()->Move(node, folder2, 0);

  // Should update the hierarchy.
  EXPECT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(folder1->children().size(), 0u);
  EXPECT_EQ(folder2->children().size(), 1u);
  EXPECT_EQ(folder2->children().front().get(), node);

  auto matches = model()->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model()->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();

  // Move back.
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer(),
                OnWillMoveBookmarkNode(folder2, 0, folder1, 0));
    EXPECT_CALL(mock_observer(), BookmarkNodeMoved(folder2, 0, folder1, 0));
  }
  model()->Move(node, folder1, 0);

  // Should update the hierarchy.
  EXPECT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(folder1->children().size(), 1u);
  EXPECT_EQ(folder2->children().size(), 0u);
  EXPECT_EQ(folder1->children().front().get(), node);

  matches = model()->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();
  matches = model()->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
}

TEST_F(BookmarkModelTest, MoveFolder) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* folder1 =
      model()->AddFolder(bookmark_bar_node, 0, u"folder");
  const BookmarkNode* folder2 =
      model()->AddFolder(bookmark_bar_node, 1, u"golder");
  const BookmarkNode* folder3 = model()->AddFolder(folder1, 0, u"holder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model()->AddURL(folder3, 0, kTitle, kUrl);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillMoveBookmarkNode(folder1, 0, folder2, 0));
  EXPECT_CALL(mock_observer(), BookmarkNodeMoved(folder1, 0, folder2, 0));

  model()->Move(folder3, folder2, 0);

  // Should update the hierarchy.
  EXPECT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0].get(), folder1);
  EXPECT_EQ(bookmark_bar_node->children()[1].get(), folder2);
  EXPECT_EQ(folder1->children().size(), 0u);
  EXPECT_EQ(folder2->children().size(), 1u);
  EXPECT_EQ(folder2->children()[0].get(), folder3);
  EXPECT_EQ(folder3->children().size(), 1u);
  EXPECT_EQ(folder3->children()[0].get(), node);

  // Should update the index.
  auto matches = model()->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
  matches = model()->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();
  matches = model()->GetBookmarksMatching(
      u"holder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
  matches.clear();
}

TEST_F(BookmarkModelTest, MoveWithUuidCollision) {
  model()->CreateAccountPermanentFolders();
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* account_bookmark_bar_node =
      model()->account_bookmark_bar_node();

  ASSERT_NE(nullptr, account_bookmark_bar_node);

  // Create two folders with the same UUID. One is local and the other one is an
  // account bookmark, so using the same UUID is allowed.
  const BookmarkNode* folder1 =
      model()->AddFolder(bookmark_bar_node, 0, u"local folder");
  const base::Uuid kInitialUuid = folder1->uuid();
  const BookmarkNode* folder2 =
      model()->AddFolder(account_bookmark_bar_node, 0, u"account folder",
                         /*meta_info=*/nullptr,
                         /*creation_time=*/std::nullopt, kInitialUuid);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(),
              OnWillMoveBookmarkNode(bookmark_bar_node, 0,
                                     account_bookmark_bar_node, 0));
  EXPECT_CALL(mock_observer(), BookmarkNodeMoved(bookmark_bar_node, 0,
                                                 account_bookmark_bar_node, 0));

  model()->Move(folder1, account_bookmark_bar_node, 0);

  // Should update the hierarchy.
  EXPECT_EQ(bookmark_bar_node->children().size(), 0u);
  EXPECT_EQ(account_bookmark_bar_node->children().size(), 2u);

  // The UUID should have changed for the moved folder.
  EXPECT_NE(folder1->uuid(), kInitialUuid);

  // The other folder involved in the collision should continue having the
  // original UUID.
  EXPECT_EQ(folder2->uuid(), kInitialUuid);

  // Verify that the UUID index was updated correctly.
  EXPECT_EQ(nullptr,
            model()->GetNodeByUuid(
                kInitialUuid, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder2, model()->GetNodeByUuid(
                         kInitialUuid, NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder1,
            model()->GetNodeByUuid(folder1->uuid(),
                                   NodeTypeForUuidLookup::kAccountNodes));
}

TEST_F(BookmarkModelTest, MoveWithUuidCollisionInDescendant) {
  model()->CreateAccountPermanentFolders();
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* account_bookmark_bar_node =
      model()->account_bookmark_bar_node();

  ASSERT_NE(nullptr, account_bookmark_bar_node);

  const BookmarkNode* folder1 =
      model()->AddFolder(bookmark_bar_node, 0, u"local folder");
  const base::Uuid kFolder1Uuid = folder1->uuid();

  // Create two more folders with the same UUID. One is local (nested under
  // `folder1`) and the other one is an account bookmark, so using the same UUID
  // is allowed.
  const BookmarkNode* folder2 = model()->AddFolder(folder1, 0, u"local folder");
  const base::Uuid kInitialUuid = folder2->uuid();
  const BookmarkNode* folder3 =
      model()->AddFolder(account_bookmark_bar_node, 0, u"account folder",
                         /*meta_info=*/nullptr,
                         /*creation_time=*/std::nullopt, kInitialUuid);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(),
              OnWillMoveBookmarkNode(bookmark_bar_node, 0,
                                     account_bookmark_bar_node, 0));
  EXPECT_CALL(mock_observer(), BookmarkNodeMoved(bookmark_bar_node, 0,
                                                 account_bookmark_bar_node, 0));

  // Move `folder1`, which also includes the nested `folder2`, to account
  // storage.
  model()->Move(folder1, account_bookmark_bar_node, 0);

  // Should update the hierarchy.
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
            model()->GetNodeByUuid(
                kInitialUuid, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder3, model()->GetNodeByUuid(
                         kInitialUuid, NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder1, model()->GetNodeByUuid(
                         kFolder1Uuid, NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder2,
            model()->GetNodeByUuid(folder2->uuid(),
                                   NodeTypeForUuidLookup::kAccountNodes));
}

// Tests the default node if no bookmarks have been added yet
TEST_F(BookmarkModelTest, ParentForNewNodesWithEmptyModel) {
  EXPECT_EQ(TestBookmarkClient::IsDesktopFormFactorByDefault()
                ? model()->other_node()
                : model()->mobile_node(),
            GetParentForNewNodes(model()));
}

#if BUILDFLAG(IS_ANDROID)
// Tests that the bookmark_bar_node can still be returned even on Android in
// case the last bookmark was added to it.
TEST_F(BookmarkModelTest, ParentCanBeBookmarkBarOnAndroid) {
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model()->AddURL(model()->bookmark_bar_node(), 0, kTitle, kUrl);
  EXPECT_EQ(model()->bookmark_bar_node(), GetParentForNewNodes(model()));
}
#endif

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewNodes) {
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model()->AddURL(model()->other_node(), 0, kTitle, kUrl);
  EXPECT_EQ(model()->other_node(), GetParentForNewNodes(model()));
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewMobileNodes) {
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model()->AddURL(model()->mobile_node(), 0, kTitle, kUrl);
  EXPECT_EQ(model()->mobile_node(), GetParentForNewNodes(model()));
}

// Make sure recently modified stays in sync when adding a URL.
TEST_F(BookmarkModelTest, MostRecentlyModifiedFolders) {
  // Add a folder.
  const BookmarkNode* folder =
      model()->AddFolder(model()->other_node(), 0, u"foo");
  // Add a URL to it.
  model()->AddURL(folder, 0, u"blah", GURL("http://foo.com"));

  // Make sure folder is in the most recently modified.
  std::vector<const BookmarkNode*> most_recent_folders =
      GetMostRecentlyModifiedUserFolders(model());
  ASSERT_FALSE(most_recent_folders.empty());
  EXPECT_EQ(folder, most_recent_folders[0]);

  // Nuke the folder and do another fetch, making sure folder isn't in the
  // returned list.
  model()->Remove(folder->parent()->children().front().get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  most_recent_folders = GetMostRecentlyModifiedUserFolders(model());
  EXPECT_FALSE(std::ranges::contains(most_recent_folders, folder));
}

// Make sure MostRecentlyAddedEntries stays in sync.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  const Time kBaseTime = Time::Now();
  BookmarkNode* n1 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 0, u"blah", GURL("http://foo.com/0")));
  BookmarkNode* n2 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 1, u"blah", GURL("http://foo.com/1")));
  BookmarkNode* n3 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 2, u"blah", GURL("http://foo.com/2")));
  BookmarkNode* n4 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 3, u"blah", GURL("http://foo.com/3")));
  n1->set_date_added(kBaseTime + base::Days(4));
  n2->set_date_added(kBaseTime + base::Days(3));
  n3->set_date_added(kBaseTime + base::Days(2));
  n4->set_date_added(kBaseTime + base::Days(1));

  // Make sure order is honored.
  std::vector<const BookmarkNode*> recently_added;
  GetMostRecentlyAddedEntries(model(), 2, &recently_added);
  EXPECT_THAT(recently_added, ElementsAre(n1, n2));

  // swap 1 and 2, then check again.
  recently_added.clear();
  SwapDateAdded(n1, n2);
  GetMostRecentlyAddedEntries(model(), 4, &recently_added);
  EXPECT_THAT(recently_added, ElementsAre(n2, n1, n3, n4));
}

// Make sure MostRecentlyAddedEntries applies across local and account
// bookmarks.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntriesLocalAndAccountBookmarks) {
  model()->CreateAccountPermanentFolders();

  // Add nodes such that the following holds for the creation time of the
  // nodes: n1 > n2 > n3.
  const Time kBaseTime = Time::Now();
  BookmarkNode* n1 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 0, u"blah", GURL("http://foo.com/1")));
  BookmarkNode* n2 =
      AsMutable(model()->AddURL(model()->account_bookmark_bar_node(), 0,
                                u"blah", GURL("http://foo.com/2")));
  BookmarkNode* n3 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 1, u"blah", GURL("http://foo.com/3")));
  n1->set_date_added(kBaseTime + base::Days(3));
  n2->set_date_added(kBaseTime + base::Days(2));
  n3->set_date_added(kBaseTime + base::Days(1));

  // Make sure order is honored across storages.
  std::vector<const BookmarkNode*> recently_added;
  GetMostRecentlyAddedEntries(model(), 3, &recently_added);
  EXPECT_THAT(recently_added, ElementsAre(n1, n2, n3));
}

// Make sure GetMostRecentlyUsedEntries stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyUsedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  const Time kBaseTime = Time::Now();
  BookmarkNode* n1 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 0, u"blah", GURL("http://foo.com/0")));
  BookmarkNode* n2 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 1, u"blah", GURL("http://foo.com/1")));
  BookmarkNode* n3 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 2, u"blah", GURL("http://foo.com/2")));
  BookmarkNode* n4 = AsMutable(model()->AddURL(
      model()->bookmark_bar_node(), 3, u"blah", GURL("http://foo.com/3")));
  n1->set_date_last_used(kBaseTime + base::Days(4));
  n2->set_date_last_used(kBaseTime + base::Days(3));
  n3->set_date_last_used(kBaseTime + base::Days(2));
  n3->set_date_added(kBaseTime + base::Days(2));
  n4->set_date_last_used(kBaseTime + base::Days(2));
  n4->set_date_added(kBaseTime + base::Days(1));

  // Make sure order is honored.
  std::vector<const BookmarkNode*> recently_used;
  GetMostRecentlyUsedEntries(model(), 2, &recently_used);
  EXPECT_THAT(recently_used, ElementsAre(n1, n2));

  // swap 1 and 2, then check again.
  recently_used.clear();
  SwapDateUsed(n1, n2);
  GetMostRecentlyUsedEntries(model(), 4, &recently_used);
  EXPECT_THAT(recently_used, ElementsAre(n2, n1, n3, n4));
}

// Makes sure GetMostRecentlyAddedUserNodeForURL stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedUserNodeForURL) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2
  const Time kBaseTime = Time::Now();
  const GURL kUrl("http://foo.com/0");
  BookmarkNode* n1 = AsMutable(
      model()->AddURL(model()->bookmark_bar_node(), 0, u"blah", kUrl));
  BookmarkNode* n2 = AsMutable(
      model()->AddURL(model()->bookmark_bar_node(), 1, u"blah", kUrl));
  n1->set_date_added(kBaseTime + base::Days(4));
  n2->set_date_added(kBaseTime + base::Days(3));

  // Make sure order is honored.
  ASSERT_EQ(n1, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  // swap 1 and 2, then check again.
  SwapDateAdded(n1, n2);
  ASSERT_EQ(n2, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));
}

// Makes sure GetUniqueUrls removes duplicates.
TEST_F(BookmarkModelTest, GetUniqueUrlsWithDups) {
  const GURL kUrl("http://foo.com/0");
  const std::u16string kTitle(u"blah");
  model()->AddURL(model()->bookmark_bar_node(), 0, kTitle, kUrl);
  model()->AddURL(model()->bookmark_bar_node(), 1, kTitle, kUrl);

  std::vector<UrlAndTitle> bookmarks = model()->GetUniqueUrls();
  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_EQ(kUrl, bookmarks[0].url);
  EXPECT_EQ(kTitle, bookmarks[0].title);

  model()->AddURL(model()->bookmark_bar_node(), 2, u"Title2", kUrl);
  // Only one returned, even titles are different.
  bookmarks = model()->GetUniqueUrls();
  EXPECT_EQ(1U, bookmarks.size());
}

TEST_F(BookmarkModelTest, HasBookmarks) {
  const GURL kUrl("http://foo.com/");
  model()->AddURL(model()->bookmark_bar_node(), 0, u"bar", kUrl);

  EXPECT_TRUE(model()->HasBookmarks());
}

TEST_F(BookmarkModelTest, Sort) {
  // Populate the bookmark bar node with nodes for 'B', 'a', 'd' and 'C'.
  // 'C' and 'a' are folders.
  TestNode bbn;
  PopulateNodeFromString("B [ a ] d [ a ]", &bbn);
  const BookmarkNode* parent = model()->bookmark_bar_node();
  PopulateBookmarkNode(&bbn, model(), parent);

  BookmarkNode* child1 = parent->children()[1].get();
  child1->SetTitle(u"a");
  child1->Remove(0);
  BookmarkNode* child3 = parent->children()[3].get();
  child3->SetTitle(u"C");
  child3->Remove(0);

  // Sort the children of the bookmark bar node.
  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillReorderBookmarkNode(parent));
  EXPECT_CALL(mock_observer(), BookmarkNodeChildrenReordered(parent));
  model()->SortChildren(parent);

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
  BookmarkNode* parent = AsMutable(model()->bookmark_bar_node());
  PopulateBookmarkNode(&bbn, model(), parent);

  // Reorder bar node's bookmarks in reverse order.
  std::vector<const BookmarkNode*> new_order = {
      parent->children()[3].get(),
      parent->children()[2].get(),
      parent->children()[1].get(),
      parent->children()[0].get(),
  };
  testing::InSequence seq;
  EXPECT_CALL(mock_observer(), OnWillReorderBookmarkNode(parent));
  EXPECT_CALL(mock_observer(), BookmarkNodeChildrenReordered(parent));
  model()->ReorderChildren(parent, new_order);

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
  BookmarkNode* parent = AsMutable(model()->bookmark_bar_node());
  PopulateBookmarkNode(&bbn, model(), parent);

  std::vector<const BookmarkNode*> same_order = {
      parent->children()[0].get(),
      parent->children()[1].get(),
      parent->children()[2].get(),
      parent->children()[3].get(),
  };
  EXPECT_CALL(mock_observer(), OnWillReorderBookmarkNode).Times(0);
  EXPECT_CALL(mock_observer(), BookmarkNodeChildrenReordered).Times(0);
  model()->ReorderChildren(parent, same_order);

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
  BookmarkNode* parent = AsMutable(model()->bookmark_bar_node());
  PopulateBookmarkNode(&bbn, model(), parent);

  std::vector<const BookmarkNode*> order_with_size_mismatch = {
      parent->children()[1].get(),
  };
  EXPECT_CALL(mock_observer(), OnWillReorderBookmarkNode).Times(0);
  EXPECT_CALL(mock_observer(), BookmarkNodeChildrenReordered).Times(0);
  model()->ReorderChildren(parent, order_with_size_mismatch);

  // Make sure the order remains the same.
  ASSERT_EQ(4u, parent->children().size());
  EXPECT_EQ("A", base::UTF16ToASCII(parent->children()[0]->GetTitle()));
  EXPECT_EQ("B", base::UTF16ToASCII(parent->children()[1]->GetTitle()));
  EXPECT_EQ("C", base::UTF16ToASCII(parent->children()[2]->GetTitle()));
  EXPECT_EQ("D", base::UTF16ToASCII(parent->children()[3]->GetTitle()));
}

TEST_F(BookmarkModelTest, NodeVisibility) {
  if (TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_TRUE(model()->bookmark_bar_node()->IsVisible());
    EXPECT_TRUE(model()->other_node()->IsVisible());
    EXPECT_FALSE(model()->mobile_node()->IsVisible());
  } else {
    EXPECT_FALSE(model()->bookmark_bar_node()->IsVisible());
    EXPECT_FALSE(model()->other_node()->IsVisible());
    EXPECT_TRUE(model()->mobile_node()->IsVisible());
  }

  // Arbitrary node should be visible
  TestNode bbn;
  PopulateNodeFromString("B", &bbn);
  const BookmarkNode* parent = model()->mobile_node();
  PopulateBookmarkNode(&bbn, model(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  parent = model()->other_node();
  PopulateBookmarkNode(&bbn, model(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  // Mobile folder should be visible now that it has a child.
  EXPECT_TRUE(model()->mobile_node()->IsVisible());
  EXPECT_TRUE(model()->other_node()->IsVisible());
}

TEST_F(BookmarkModelTest, NodeVisibility_AddBookmarkToNonVisibleFolder) {
  const BookmarkPermanentNode* permanent_folder =
      TestBookmarkClient::IsDesktopFormFactorByDefault()
          ? model()->mobile_node()
          : model()->other_node();

  // This permanent folder is not visible when empty.
  ASSERT_FALSE(permanent_folder->IsVisible());

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
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
  model()->AddURL(permanent_folder, 0, u"Title", GURL("http://foo.com"));
  EXPECT_TRUE(permanent_folder->IsVisible());
}

#if !((BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)) || \
      BUILDFLAG(IS_IOS))
TEST_F(BookmarkModelTest, NodeVisibility_AddFirstLocalBookmarkToOtherFolder) {
  model()->CreateAccountPermanentFolders();

  // To start with, only the account permanent folders are visible.
  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model()->account_bookmark_bar_node(),
                          model()->account_other_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model()->other_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model()->other_node(),
                                model()->account_bookmark_bar_node(),
                                model()->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer, BookmarkNodeAdded(model()->other_node(), _, _))
      .WillOnce(WithArg<0>([this](const BookmarkNode* parent) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model()->other_node(),
                                model()->account_bookmark_bar_node(),
                                model()->account_other_node()));
      }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(
                            model()->bookmark_bar_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(model()->bookmark_bar_node(), model()->other_node(),
                        model()->account_bookmark_bar_node(),
                        model()->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });

  // Add a bookmark to the local mobile folder.
  model()->AddURL(model()->other_node(), 0, u"Title", GURL("http://foo.com"));
  EXPECT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model()->bookmark_bar_node(), model()->other_node(),
                          model()->account_bookmark_bar_node(),
                          model()->account_other_node()));
}

TEST_F(BookmarkModelTest, NodeVisibility_AddFirstLocalBookmarkToMobileFolder) {
  model()->CreateAccountPermanentFolders();

  // To start with, only the account permanent folders are visible.
  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model()->account_bookmark_bar_node(),
                          model()->account_other_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model()->mobile_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model()->mobile_node(),
                                model()->account_bookmark_bar_node(),
                                model()->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer, BookmarkNodeAdded(model()->mobile_node(), _, _))
      .WillOnce(WithArg<0>([this](const BookmarkNode* parent) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model()->mobile_node(),
                                model()->account_bookmark_bar_node(),
                                model()->account_other_node()));
      }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(
                            model()->bookmark_bar_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(model()->bookmark_bar_node(), model()->mobile_node(),
                        model()->account_bookmark_bar_node(),
                        model()->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model()->other_node()))
      .WillOnce([this](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model()->bookmark_bar_node(),
                                model()->other_node(), model()->mobile_node(),
                                model()->account_bookmark_bar_node(),
                                model()->account_other_node()));
        EXPECT_THAT(node->children(), IsEmpty());
      });

  // Add a bookmark to the local mobile folder.
  model()->AddURL(model()->mobile_node(), 0, u"Title", GURL("http://foo.com"));
  EXPECT_THAT(
      GetVisiblePermanentNodes(),
      ElementsAre(model()->bookmark_bar_node(), model()->other_node(),
                  model()->mobile_node(), model()->account_bookmark_bar_node(),
                  model()->account_other_node()));
}
#endif  // !((BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)) ||
        // BUILDFLAG(IS_IOS))

TEST_F(BookmarkModelTest, NodeVisibility_RemoveLastBookmarkFromVisibleFolder) {
  const BookmarkPermanentNode* permanent_folder =
      TestBookmarkClient::IsDesktopFormFactorByDefault()
          ? model()->mobile_node()
          : model()->other_node();

  // This permanent folder is not visible when empty; visible when non-empty.
  ASSERT_FALSE(permanent_folder->IsVisible());
  const BookmarkNode* bookmark =
      model()->AddURL(permanent_folder, 0, u"Title", GURL("http://foo.com"));
  ASSERT_TRUE(permanent_folder->IsVisible());

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
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
  model()->Remove(bookmark, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);
  EXPECT_FALSE(permanent_folder->IsVisible());
}

#if !((BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)) || \
      BUILDFLAG(IS_IOS))
TEST_F(BookmarkModelTest,
       NodeVisibility_MoveBookmarkChangesVisibilityOfSourceFolder) {
  const BookmarkPermanentNode* source_folder = model()->mobile_node();

  // This permanent folder is not visible when empty; visible when non-empty.
  ASSERT_FALSE(source_folder->IsVisible());
  ASSERT_TRUE(model()->bookmark_bar_node()->IsVisible());
  const BookmarkNode* bookmark =
      model()->AddURL(source_folder, 0, u"Title", GURL("http://foo.com"));
  ASSERT_TRUE(source_folder->IsVisible());

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
  EXPECT_CALL(observer, OnWillMoveBookmarkNode(
                            source_folder, 0, model()->bookmark_bar_node(), 0));
  EXPECT_CALL(observer, BookmarkNodeMoved(source_folder, 0,
                                          model()->bookmark_bar_node(), 0))
      .WillOnce(WithArg<0>(
          [](const BookmarkNode* node) { EXPECT_TRUE(node->IsVisible()); }));
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(source_folder))
      .WillOnce([](const BookmarkPermanentNode* node) {
        EXPECT_FALSE(node->IsVisible());
      });

  // Remove the bookmark.
  model()->Move(bookmark, model()->bookmark_bar_node(), 0);
  EXPECT_FALSE(source_folder->IsVisible());
}

TEST_F(BookmarkModelTest,
       NodeVisibility_MoveBookmarkChangesVisibilityOfDestinationFolder) {
  const BookmarkPermanentNode* source_folder = model()->other_node();
  const BookmarkPermanentNode* destination_folder = model()->mobile_node();

  // This source folder is visible when empty, destination folder is not.
  ASSERT_TRUE(source_folder->IsVisible());
  ASSERT_FALSE(destination_folder->IsVisible());
  const BookmarkNode* bookmark =
      model()->AddURL(source_folder, 0, u"Title", GURL("http://foo.com"));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
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
  model()->Move(bookmark, destination_folder, 0);
  EXPECT_TRUE(destination_folder->IsVisible());
}

TEST_F(
    BookmarkModelTest,
    NodeVisibility_MoveBookmarkChangesVisibilityOfSourceAndDestinationFolder) {
  // Create empty account folders.
  model()->CreateAccountPermanentFolders();

  const BookmarkPermanentNode* source_folder = model()->account_mobile_node();
  const BookmarkPermanentNode* destination_folder = model()->mobile_node();

  // Add a node to the local bookmark bar, to simplify the test by avoiding all
  // local folders becoming invisible.
  model()->AddURL(model()->bookmark_bar_node(), 0, u"Title",
                  GURL("http://bar.com"));
  const BookmarkNode* bookmark =
      model()->AddURL(source_folder, 0, u"Title", GURL("http://foo.com"));
  ASSERT_TRUE(source_folder->IsVisible());
  ASSERT_FALSE(destination_folder->IsVisible());

  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model()->bookmark_bar_node(), model()->other_node(),
                          model()->account_bookmark_bar_node(),
                          model()->account_other_node(),
                          model()->account_mobile_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());

  // Expect first a callback that the folder is visible, then that the child
  // bookmark is added.
  testing::InSequence seq;
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
  model()->Move(bookmark, destination_folder, 0);
  EXPECT_FALSE(source_folder->IsVisible());
  EXPECT_TRUE(destination_folder->IsVisible());
}

TEST_F(BookmarkModelTest, NodeVisibility_CreateAccountPermanentFolders) {
  // Check per-platform visibility of empty permanent nodes.
  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model()->bookmark_bar_node(), model()->other_node()));

  testing::StrictMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());
  testing::InSequence seq;

  // First expect callbacks for the previously-visible local permanent folders.
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(
                            model()->bookmark_bar_node()))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(model()->other_node()));
      });
  EXPECT_CALL(observer,
              BookmarkPermanentNodeVisibilityChanged(model()->other_node()))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_THAT(GetVisiblePermanentNodes(), IsEmpty());
      });

  // Now expect callbacks for the newly-visible account permanent folders.
  EXPECT_CALL(observer, BookmarkNodeAdded(_, 3, _))
      .WillOnce(
          WithArgs<0, 1>([this](const BookmarkNode* parent, size_t index) {
            const BookmarkNode* node = parent->children()[index].get();
            EXPECT_EQ(node, model()->account_bookmark_bar_node());
            EXPECT_TRUE(node->IsVisible());
            EXPECT_THAT(GetVisiblePermanentNodes(),
                        ElementsAre(model()->account_bookmark_bar_node()));
          }));

  EXPECT_CALL(observer, BookmarkNodeAdded(_, 4, _))
      .WillOnce(
          WithArgs<0, 1>([this](const BookmarkNode* parent, size_t index) {
            const BookmarkNode* node = parent->children()[index].get();
            EXPECT_EQ(node, model()->account_other_node());
            EXPECT_TRUE(node->IsVisible());
            EXPECT_THAT(GetVisiblePermanentNodes(),
                        ElementsAre(model()->account_bookmark_bar_node(),
                                    model()->account_other_node()));
          }));

  EXPECT_CALL(observer, BookmarkNodeAdded(_, 5, _))
      .WillOnce(
          WithArgs<0, 1>([this](const BookmarkNode* parent, size_t index) {
            const BookmarkNode* node = parent->children()[index].get();
            EXPECT_EQ(node, model()->account_mobile_node());
            EXPECT_FALSE(node->IsVisible());
            EXPECT_THAT(GetVisiblePermanentNodes(),
                        ElementsAre(model()->account_bookmark_bar_node(),
                                    model()->account_other_node()));
          }));

  // Create empty account folders.
  model()->CreateAccountPermanentFolders();
  EXPECT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(model()->account_bookmark_bar_node(),
                          model()->account_other_node()));
}

TEST_F(BookmarkModelTest, NodeVisibility_RemoveAccountPermanentFolders) {
  // Create empty account folders.
  model()->CreateAccountPermanentFolders();
  const BookmarkPermanentNode* local_bb = model()->bookmark_bar_node();
  const BookmarkPermanentNode* local_other = model()->other_node();
  const BookmarkPermanentNode* account_bb =
      model()->account_bookmark_bar_node();
  const BookmarkPermanentNode* account_other = model()->account_other_node();

  ASSERT_THAT(GetVisiblePermanentNodes(),
              ElementsAre(account_bb, account_other));

  // Set up observer expectations.
  testing::NiceMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(model());
  testing::InSequence seq;

  // Then expect callbacks for the previously-invisible local permanent
  // folders.
  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(local_bb))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_TRUE(model()->IsLocalOnlyNode(*node));
        EXPECT_TRUE(node->IsVisible());
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(local_bb, account_bb, account_other));
      });

  EXPECT_CALL(observer, BookmarkPermanentNodeVisibilityChanged(local_other))
      .WillOnce([&](const BookmarkPermanentNode* node) {
        EXPECT_TRUE(model()->IsLocalOnlyNode(*node));
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
              BookmarkNodeRemoved(_, _, model()->account_mobile_node(), _, _))
      .WillOnce(WithArg<2>([&](const BookmarkNode* node) {
        EXPECT_FALSE(model()->IsLocalOnlyNode(*node));
        EXPECT_FALSE(node->IsVisible());
        EXPECT_THAT(
            GetVisiblePermanentNodes(),
            ElementsAre(local_bb, local_other, account_bb, account_other));
      }));

  EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, account_other, _, _))
      .WillOnce(WithArgs<1, 2>([&](size_t old_index, const BookmarkNode* node) {
        EXPECT_FALSE(model()->IsLocalOnlyNode(*node));
        EXPECT_EQ(GetVisibleIndexForPermanentNode(old_index), 3u);
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(local_bb, local_other, account_bb));
      }));

  EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, account_bb, _, _))
      .WillOnce(WithArgs<1, 2>([&](size_t old_index, const BookmarkNode* node) {
        EXPECT_FALSE(model()->IsLocalOnlyNode(*node));
        EXPECT_EQ(GetVisibleIndexForPermanentNode(old_index), 2u);
        EXPECT_THAT(GetVisiblePermanentNodes(),
                    ElementsAre(local_bb, local_other));
      }));

  // Remove the account folders.
  model()->RemoveAccountPermanentFolders();
  EXPECT_THAT(GetVisiblePermanentNodes(), ElementsAre(local_bb, local_other));
}
#endif  // !((BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)) ||
        // BUILDFLAG(IS_IOS))

TEST_F(BookmarkModelTest, NodeVisibility_AllBookmarksPhase0) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      bookmarks::kAllBookmarksBaselineFolderVisibility);
  ResetModelWithClient(std::make_unique<TestBookmarkClientWithUndo>());
  if (TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_TRUE(model()->bookmark_bar_node()->IsVisible());
  } else {
    EXPECT_FALSE(model()->bookmark_bar_node()->IsVisible());
  }

  EXPECT_TRUE(model()->other_node()->IsVisible());
  // EXPECT_FALSE(model()->mobile_node()->IsVisible());

  // Arbitrary node should be visible
  TestNode bbn;
  PopulateNodeFromString("B", &bbn);
  const BookmarkNode* parent = model()->mobile_node();
  PopulateBookmarkNode(&bbn, model(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());
  parent = model()->other_node();
  PopulateBookmarkNode(&bbn, model(), parent);
  EXPECT_TRUE(parent->children().front()->IsVisible());

  // Mobile folder should be visible now that it has a child.
  EXPECT_TRUE(model()->mobile_node()->IsVisible());
  EXPECT_TRUE(model()->other_node()->IsVisible());
}

TEST_F(BookmarkModelTest, MobileNodeVisibleWithChildren) {
  const BookmarkNode* mobile_node = model()->mobile_node();
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");

  model()->AddURL(mobile_node, 0, kTitle, kUrl);
  EXPECT_TRUE(model()->mobile_node()->IsVisible());
}

TEST_F(BookmarkModelTest, ExtensiveChangesObserver) {
  EXPECT_FALSE(model()->IsDoingExtensiveChanges());

  EXPECT_CALL(mock_observer(), ExtensiveBookmarkChangesBeginning());
  model()->BeginExtensiveChanges();
  EXPECT_TRUE(model()->IsDoingExtensiveChanges());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  EXPECT_CALL(mock_observer(), ExtensiveBookmarkChangesEnded());
  model()->EndExtensiveChanges();
  EXPECT_FALSE(model()->IsDoingExtensiveChanges());
}

TEST_F(BookmarkModelTest, MultipleExtensiveChangesObserver) {
  EXPECT_FALSE(model()->IsDoingExtensiveChanges());

  EXPECT_CALL(mock_observer(), ExtensiveBookmarkChangesBeginning());
  model()->BeginExtensiveChanges();
  EXPECT_TRUE(model()->IsDoingExtensiveChanges());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer()));

  model()->BeginExtensiveChanges();
  EXPECT_TRUE(model()->IsDoingExtensiveChanges());
  model()->EndExtensiveChanges();
  EXPECT_TRUE(model()->IsDoingExtensiveChanges());

  EXPECT_CALL(mock_observer(), ExtensiveBookmarkChangesEnded());
  model()->EndExtensiveChanges();
  EXPECT_FALSE(model()->IsDoingExtensiveChanges());
}

// Verifies that IsBookmarked is true if any bookmark matches the given URL,
// and that IsBookmarkedByUser is true only if at least one of the matching
// bookmarks can be edited by the user.
TEST_F(BookmarkModelTest, IsBookmarked) {
  // Reload the model with a managed node that is not editable by the user.
  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();

  // "google.com" is a "user" bookmark.
  model()->AddURL(model()->other_node(), 0, u"User", GURL("http://google.com"));
  // "youtube.com" is not.
  model()->AddURL(managed_node, 0, u"Managed", GURL("http://youtube.com"));

  EXPECT_TRUE(model()->IsBookmarked(GURL("http://google.com")));
  EXPECT_TRUE(model()->IsBookmarked(GURL("http://youtube.com")));
  EXPECT_FALSE(model()->IsBookmarked(GURL("http://reddit.com")));

  EXPECT_TRUE(IsBookmarkedByUser(model(), GURL("http://google.com")));
  EXPECT_FALSE(IsBookmarkedByUser(model(), GURL("http://youtube.com")));
  EXPECT_FALSE(IsBookmarkedByUser(model(), GURL("http://reddit.com")));
}

// Verifies that GetMostRecentlyAddedUserNodeForURL skips bookmarks that
// are not owned by the user.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedUserNodeForURLSkipsManagedNodes) {
  // Reload the model with a managed node that is not editable by the user.
  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();

  const std::u16string kTitle = u"Title";
  const BookmarkNode* user_parent = model()->other_node();
  const BookmarkNode* managed_parent = managed_node;
  const GURL kUrl("http://google.com");

  // `url` is not bookmarked yet.
  EXPECT_EQ(model()->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);

  // Having a managed node doesn't count.
  model()->AddURL(managed_parent, 0, kTitle, kUrl);
  EXPECT_EQ(model()->GetMostRecentlyAddedUserNodeForURL(kUrl), nullptr);

  // Now add a user node.
  const BookmarkNode* user = model()->AddURL(user_parent, 0, kTitle, kUrl);
  EXPECT_EQ(user, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));

  // Having a more recent managed node doesn't count either.
  const BookmarkNode* managed =
      model()->AddURL(managed_parent, 0, kTitle, kUrl);
  EXPECT_GE(managed->date_added(), user->date_added());
  EXPECT_EQ(user, model()->GetMostRecentlyAddedUserNodeForURL(kUrl));
}

// Verifies that renaming a bookmark folder does not add the folder node to the
// autocomplete index. crbug.com/778266
TEST_F(BookmarkModelTest, RenamedFolderNodeExcludedFromIndex) {
  // Add a folder.
  const BookmarkNode* folder =
      model()->AddFolder(model()->other_node(), 0, u"MyFavorites");

  // Change the folder title.
  model()->SetTitle(folder, u"MyBookmarks",
                    metrics::BookmarkEditSource::kOther);

  // There should be no matching bookmarks.
  std::vector<TitledUrlMatch> matches = model()->GetBookmarksMatching(
      u"MyB", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());
}

TEST_F(BookmarkModelTest, GetBookmarksMatching) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* folder =
      model()->AddFolder(bookmark_bar_node, 0, u"folder");
  const std::u16string kTitle(u"foo");
  const GURL kUrl("http://foo.com");
  const BookmarkNode* node = model()->AddURL(folder, 0, kTitle, kUrl);

  // Should not match incorrect paths.
  auto matches = model()->GetBookmarksMatching(
      u"golder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_TRUE(matches.empty());

  // Should match correct paths.
  matches = model()->GetBookmarksMatching(
      u"folder foo", /*max_count=*/1, query_parser::MatchingAlgorithm::DEFAULT);
  EXPECT_EQ(matches[0].node, node);
}

// Verifies that TitledUrlIndex is updated when a bookmark is removed.
TEST_F(BookmarkModelTest, TitledUrlIndexUpdatedOnRemove) {
  const std::u16string kTitle = u"Title";
  const GURL kUrl("http://google.com");
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  model()->AddURL(bookmark_bar_node, 0, kTitle, kUrl);
  ASSERT_EQ(1U, model()
                    ->GetBookmarksMatching(
                        kTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());

  // Remove the node and make sure we don't get back any results.
  model()->Remove(bookmark_bar_node->children().front().get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_EQ(0U, model()
                    ->GetBookmarksMatching(
                        kTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
}

// Verifies that TitledUrlIndex is updated when a bookmark's title changes.
TEST_F(BookmarkModelTest, TitledUrlIndexUpdatedOnChangeTitle) {
  const std::u16string kInitialTitle = u"Initial";
  const std::u16string kNewTitle = u"New";
  const GURL kUrl("http://google.com");
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  model()->AddURL(bookmark_bar_node, 0, kInitialTitle, kUrl);
  ASSERT_EQ(1U,
            model()
                ->GetBookmarksMatching(kInitialTitle, 1,
                                       query_parser::MatchingAlgorithm::DEFAULT)
                .size());
  ASSERT_EQ(0U, model()
                    ->GetBookmarksMatching(
                        kNewTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());

  // Change the title.
  model()->SetTitle(bookmark_bar_node->children().front().get(), kNewTitle,
                    metrics::BookmarkEditSource::kOther);

  // Verify that we only get results for the new title.
  EXPECT_EQ(0U,
            model()
                ->GetBookmarksMatching(kInitialTitle, 1,
                                       query_parser::MatchingAlgorithm::DEFAULT)
                .size());
  EXPECT_EQ(1U, model()
                    ->GetBookmarksMatching(
                        kNewTitle, 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
}

// Verifies that TitledUrlIndex is updated when a bookmark's URL changes.
TEST_F(BookmarkModelTest, TitledUrlIndexUpdatedOnChangeURL) {
  const std::u16string kTitle = u"Title";
  const GURL kInitialUrl("http://initial");
  const GURL kNewUr("http://new");
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  model()->AddURL(bookmark_bar_node, 0, kTitle, kInitialUrl);
  ASSERT_EQ(1U, model()
                    ->GetBookmarksMatching(
                        u"initial", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
  ASSERT_EQ(0U, model()
                    ->GetBookmarksMatching(
                        u"new", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());

  // Change the URL.
  model()->SetURL(bookmark_bar_node->children().front().get(), kNewUr,
                  metrics::BookmarkEditSource::kOther);

  // Verify that we only get results for the new URL.
  EXPECT_EQ(0U, model()
                    ->GetBookmarksMatching(
                        u"initial", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
  EXPECT_EQ(1U, model()
                    ->GetBookmarksMatching(
                        u"new", 1, query_parser::MatchingAlgorithm::DEFAULT)
                    .size());
}

TEST_F(BookmarkModelTest, GetNodeByUuid) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const base::Uuid kExplicitUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kExplicitUuid2 = base::Uuid::GenerateRandomV4();

  // Create two nodes (URL and folder) without specifying a UUID, which means
  // a random one is used.
  const BookmarkNode* url_node_with_implicit_uuid =
      model()->AddURL(bookmark_bar_node, 0, u"title", GURL("http://foo.com"));
  const BookmarkNode* folder_node_with_implicit_uuid =
      model()->AddFolder(bookmark_bar_node, 0, u"title");

  // Create two more with an explicit UUID provided.
  const BookmarkNode* url_node_with_explicit_uuid = model()->AddURL(
      bookmark_bar_node, 0, u"title", GURL("http://foo.com"),
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, kExplicitUuid1);
  const BookmarkNode* folder_node_with_explicit_uuid = model()->AddFolder(
      bookmark_bar_node, 0, u"title",
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, kExplicitUuid2);

  ASSERT_TRUE(url_node_with_implicit_uuid);
  ASSERT_TRUE(folder_node_with_implicit_uuid);
  ASSERT_TRUE(url_node_with_explicit_uuid);
  ASSERT_TRUE(folder_node_with_explicit_uuid);

  EXPECT_EQ(
      url_node_with_implicit_uuid,
      model()->GetNodeByUuid(url_node_with_implicit_uuid->uuid(),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      folder_node_with_implicit_uuid,
      model()->GetNodeByUuid(folder_node_with_implicit_uuid->uuid(),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(url_node_with_explicit_uuid,
            model()->GetNodeByUuid(
                kExplicitUuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder_node_with_explicit_uuid,
            model()->GetNodeByUuid(
                kExplicitUuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Verify cases that should return nullptr.
  EXPECT_EQ(nullptr,
            model()->GetNodeByUuid(
                base::Uuid(), NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::GenerateRandomV4(),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetAccountNodeByUuid) {
  model()->CreateAccountPermanentFolders();
  const BookmarkNode* account_bookmark_bar_node =
      model()->account_bookmark_bar_node();
  ASSERT_NE(nullptr, account_bookmark_bar_node);
  ASSERT_EQ(account_bookmark_bar_node,
            model()->GetNodeByUuid(account_bookmark_bar_node->uuid(),
                                   NodeTypeForUuidLookup::kAccountNodes));
  ASSERT_NE(
      account_bookmark_bar_node,
      model()->GetNodeByUuid(account_bookmark_bar_node->uuid(),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Create two nodes (URL and folder) without specifying a UUID, which means
  // a random one is used.
  const BookmarkNode* url_node_with_implicit_uuid = model()->AddURL(
      account_bookmark_bar_node, 0, u"title", GURL("http://foo.com"));
  const BookmarkNode* folder_node_with_implicit_uuid =
      model()->AddFolder(account_bookmark_bar_node, 0, u"title");

  ASSERT_TRUE(url_node_with_implicit_uuid);
  ASSERT_TRUE(folder_node_with_implicit_uuid);

  EXPECT_EQ(url_node_with_implicit_uuid,
            model()->GetNodeByUuid(url_node_with_implicit_uuid->uuid(),
                                   NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(folder_node_with_implicit_uuid,
            model()->GetNodeByUuid(folder_node_with_implicit_uuid->uuid(),
                                   NodeTypeForUuidLookup::kAccountNodes));

  // Verify that lookups using kLocalOrSyncableNodes return nullptr.
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         url_node_with_implicit_uuid->uuid(),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         folder_node_with_implicit_uuid->uuid(),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetPermanentNodeByUuid) {
  // Permanent nodes should be returned by UUID.
  EXPECT_EQ(
      model()->root_node(),
      model()->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      model()->bookmark_bar_node(),
      model()->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(model()->mobile_node(),
            model()->GetNodeByUuid(
                base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(model()->other_node(),
            model()->GetNodeByUuid(
                base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Managed bookmarks don't exist by default.
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kManagedNodeUuid),
                         NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();
  EXPECT_EQ(managed_node, model()->GetNodeByUuid(
                              base::Uuid::ParseLowercase(kManagedNodeUuid),
                              NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetAccountPermanentNodeByUuid) {
  BookmarkPermanentNode* managed_node = ReloadModelWithManagedNode();
  ASSERT_NE(nullptr, managed_node);

  ASSERT_EQ(nullptr, model()->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model()->account_mobile_node());
  ASSERT_EQ(nullptr, model()->account_other_node());

  // Before account nodes are created, the lookups should return null.
  EXPECT_EQ(nullptr,
            model()->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                                   NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr,
            model()->GetNodeByUuid(base::Uuid::ParseLowercase(kManagedNodeUuid),
                                   NodeTypeForUuidLookup::kAccountNodes));

  model()->CreateAccountPermanentFolders();
  ASSERT_NE(nullptr, model()->account_bookmark_bar_node());
  ASSERT_NE(nullptr, model()->account_mobile_node());
  ASSERT_NE(nullptr, model()->account_other_node());

  // The root node's UUID should still return null.
  EXPECT_EQ(nullptr,
            model()->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                                   NodeTypeForUuidLookup::kAccountNodes));

  // Account permanent nodes should be returned by UUID.
  EXPECT_EQ(
      model()->account_bookmark_bar_node(),
      model()->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                             NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(model()->account_mobile_node(),
            model()->GetNodeByUuid(
                base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(model()->account_other_node(),
            model()->GetNodeByUuid(
                base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                NodeTypeForUuidLookup::kAccountNodes));

  // Managed bookmarks are not considered account nodes.
  EXPECT_EQ(nullptr,
            model()->GetNodeByUuid(base::Uuid::ParseLowercase(kManagedNodeUuid),
                                   NodeTypeForUuidLookup::kAccountNodes));

  // Verify that having created account bookmarks doesn't influence
  // local-or-syncable lookups.
  ASSERT_NE(model()->bookmark_bar_node(), model()->account_bookmark_bar_node());
  ASSERT_NE(model()->mobile_node(), model()->account_mobile_node());
  ASSERT_NE(model()->other_node(), model()->account_other_node());
  EXPECT_EQ(
      model()->root_node(),
      model()->GetNodeByUuid(base::Uuid::ParseLowercase(kRootNodeUuid),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      model()->bookmark_bar_node(),
      model()->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(model()->mobile_node(),
            model()->GetNodeByUuid(
                base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(model()->other_node(),
            model()->GetNodeByUuid(
                base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Removing account nodes should make lookups fail again.
  model()->RemoveAccountPermanentFolders();
  ASSERT_EQ(nullptr, model()->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model()->account_mobile_node());
  ASSERT_EQ(nullptr, model()->account_other_node());
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
                         NodeTypeForUuidLookup::kAccountNodes));
}

TEST_F(BookmarkModelTest, GetNodeByUuidAfterRemove) {
  const BookmarkNode* folder1 =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"title1");
  const BookmarkNode* folder2 =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"title2");

  const base::Uuid uuid1 = folder1->uuid();
  const base::Uuid uuid2 = folder2->uuid();

  ASSERT_EQ(folder1, model()->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_EQ(folder2, model()->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  model()->Remove(folder1, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);

  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(folder2, model()->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  model()->Remove(folder2, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);

  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetNodeByUuidAfterRemoveAllUserBookmarks) {
  const BookmarkNode* folder1 =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"title1");
  const BookmarkNode* folder2 =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"title2");

  const base::Uuid uuid1 = folder1->uuid();
  const base::Uuid uuid2 = folder2->uuid();

  ASSERT_EQ(folder1, model()->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_EQ(folder2, model()->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  model()->RemoveAllUserBookmarks(FROM_HERE);

  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         uuid1, NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model()->GetNodeByUuid(
                         uuid2, NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Permanent nodes should continue to exist and be returned by UUID.
  EXPECT_EQ(
      model()->bookmark_bar_node(),
      model()->GetNodeByUuid(base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
                             NodeTypeForUuidLookup::kLocalOrSyncableNodes));
}

TEST_F(BookmarkModelTest, GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes) {
  constexpr size_t kOriginalNumberOfNodes = 4u;
  EXPECT_EQ(kOriginalNumberOfNodes,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a folder.
  const BookmarkNode* folder =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"title");
  EXPECT_EQ(kOriginalNumberOfNodes + 1,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a URL in the folder.
  model()->AddURL(folder, 0, u"title", GURL("http://foo.com"));
  EXPECT_EQ(kOriginalNumberOfNodes + 2,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Remove the bookmark URL.
  model()->RemoveLastChild(
      folder, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_EQ(kOriginalNumberOfNodes + 1,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Add a bookmark in the bookmark bar.
  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"url",
                     GURL("http://url.com"));
  EXPECT_EQ(kOriginalNumberOfNodes + 2,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Create account folders.
  model()->CreateAccountPermanentFolders();
  EXPECT_EQ(kOriginalNumberOfNodes + 5,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());

  // Delete everything.
  model()->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_EQ(kOriginalNumberOfNodes + 3,
            model()->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());
}

class BookmarkModelLoadTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  BookmarkModelLoadTest() {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
  }

  os_crypt_async::OSCryptAsync* os_crypt_async() {
    return os_crypt_async_.get();
  }

  std::unique_ptr<BookmarkModel> CreateBookmarkModel() {
    return std::make_unique<BookmarkModel>(
        std::make_unique<TestBookmarkClient>(os_crypt_async()));
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting();
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(BookmarkModelLoadTest, NodesPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  const GURL node_url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, u"User", node_url);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  base::HistogramTester histogram_tester;

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  ASSERT_EQ(1u, model->bookmark_bar_node()->children().size());
  EXPECT_EQ(node_url, model->bookmark_bar_node()->children()[0]->url());

  histogram_tester.ExpectTotalCount("Bookmarks.Storage.TimeToLoadAtStartup2",
                                    1);
}

TEST_P(BookmarkModelLoadTest, NodesPopulatedIncludingAccountNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with one local-or-syncable url and one account url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
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
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  ASSERT_NE(nullptr, model->account_bookmark_bar_node());
  ASSERT_EQ(1u, model->bookmark_bar_node()->children().size());
  ASSERT_EQ(1u, model->account_bookmark_bar_node()->children().size());
  EXPECT_EQ(node_url1, model->bookmark_bar_node()->children()[0]->url());
  EXPECT_EQ(node_url2,
            model->account_bookmark_bar_node()->children()[0]->url());
}

TEST_P(BookmarkModelLoadTest, AccountSyncMetadataPopulatedWithoutNodesOnLoad) {
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
    auto client = std::make_unique<TestBookmarkClient>(os_crypt_async());
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
  auto client = std::make_unique<TestBookmarkClient>(os_crypt_async());
  TestBookmarkClient* client_ptr = client.get();
  BookmarkModel model(std::move(client));
  model.Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(&model);

  EXPECT_EQ(nullptr, model.account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model.account_other_node());
  EXPECT_EQ(nullptr, model.account_mobile_node());

  EXPECT_EQ(sync_metadata_str, client_ptr->account_bookmark_sync_metadata());
}

TEST_P(BookmarkModelLoadTest,
       RemoveAccountPermanentFoldersUponMetadataDecoding) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with account bookmarks.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  {
    auto model = CreateBookmarkModel();
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
  auto client = std::make_unique<TestBookmarkClient>(os_crypt_async());
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
TEST_P(BookmarkModelLoadTest, TitledUrlIndexPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  const GURL node_url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, u"User", node_url);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  std::vector<TitledUrlMatch> matches = model->GetBookmarksMatching(
      u"user", 1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(node_url, matches[0].node->GetTitledUrlNodeUrl());
}

// Verifies the TitledUrlIndex is properly loaded for account bookmarks.
TEST_P(BookmarkModelLoadTest, TitledUrlIndexPopulatedForAccountNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->CreateAccountPermanentFolders();

  const GURL node_url("http://google.com");
  model->AddURL(model->account_bookmark_bar_node(), 0, u"User", node_url);

  // This is necessary to ensure the save completes.
  task_environment.FastForwardUntilNoTasksRemain();

  // Recreate the model and ensure GetBookmarksMatching() returns the url that
  // was added.
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  std::vector<TitledUrlMatch> matches = model->GetBookmarksMatching(
      u"user", 1, query_parser::MatchingAlgorithm::DEFAULT);
  ASSERT_EQ(1u, matches.size());
  EXPECT_EQ(node_url, matches[0].node->GetTitledUrlNodeUrl());
}

// Verifies the UUID index is properly loaded.
TEST_P(BookmarkModelLoadTest, UuidIndexPopulatedOnLoad) {
  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
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
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  EXPECT_NE(nullptr,
            model->GetNodeByUuid(node_uuid,
                                 NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model->GetNodeByUuid(
                         node_uuid, NodeTypeForUuidLookup::kAccountNodes));
}

// Verifies the UUID index is properly loaded, for account nodes.
TEST_P(BookmarkModelLoadTest, UuidIndexPopulatedForAccountNodesOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
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
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  EXPECT_EQ(nullptr,
            model->GetNodeByUuid(node_uuid,
                                 NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_NE(nullptr, model->GetNodeByUuid(
                         node_uuid, NodeTypeForUuidLookup::kAccountNodes));
}

TEST_P(BookmarkModelLoadTest,
       GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // Create a model with a single url.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
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
  model = CreateBookmarkModel();
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

INSTANTIATE_TEST_SUITE_P(
    BookmarkModelLoadTest,
    BookmarkModelLoadTest,
    ::testing::ValuesIn(test::kAllBookmarkEncryptionStages));

TEST(BookmarkModelUnencryptedStorageTest, SaveExactlyOneUnencryptedFile) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({switches::kSyncEnableBookmarksInTransportMode},
                            {kEncryptBookmarks});

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

// TODO(crbug.com/435317726): Remove this function once encryption is fully
// rolled out.
void AssertSameFileContent(const base::FilePath& unencrypted_file_path,
                           const base::FilePath& encrypted_file_path,
                           BookmarkClient* client) {
  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(unencrypted_file_path, &file_content));

  std::string encrypted_file_content;
  ASSERT_TRUE(
      base::ReadFileToString(encrypted_file_path, &encrypted_file_content));

  std::string decrypted_file_content;
  base::test::TestFuture<os_crypt_async::Encryptor> future;
  client->GetEncryptor(future.GetCallback());
  os_crypt_async::Encryptor encryptor = future.Take();
  ASSERT_TRUE(
      encryptor.DecryptString(encrypted_file_content, &decrypted_file_content));

  EXPECT_FALSE(file_content.empty());
  EXPECT_EQ(file_content, decrypted_file_content);
}

class BookmarkModelStorageWithSecondayFileTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  BookmarkModelStorageWithSecondayFileTest() {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
  }

  std::unique_ptr<BookmarkModel> CreateBookmarkModel() {
    return std::make_unique<BookmarkModel>(
        std::make_unique<TestBookmarkClient>(os_crypt_async_.get()));
  }

  const base::FilePath GetLocalOrSyncableSecondaryFilename() {
    return IsEncryptedFilePrimary()
               ? base::FilePath(kLocalOrSyncableBookmarksFileName)
               : base::FilePath(kEncryptedLocalOrSyncableBookmarksFileName);
  }

  const base::FilePath GetAccountSecondaryFilename() {
    return IsEncryptedFilePrimary()
               ? base::FilePath(kAccountBookmarksFileName)
               : base::FilePath(kEncryptedAccountBookmarksFileName);
  }

  std::string GetPrimaryHistogramName(std::string_view histogram_name_prefix) {
    return base::StrCat({histogram_name_prefix, IsEncryptedFilePrimary()
                                                    ? ".Encrypted"
                                                    : ".ClearText"});
  }

  std::string GetSecondaryHistogramName(
      std::string_view histogram_name_prefix) {
    return base::StrCat({histogram_name_prefix, IsEncryptedFilePrimary()
                                                    ? ".ClearText"
                                                    : ".Encrypted"});
  }

  std::string GetSecondaryBookmarkStorageHistogramSuffix() {
    return IsEncryptedFilePrimary() ? ".BookmarkStorageImmediate"
                                    : ".BookmarkStorageEncryptedImmediate";
  }

  bool IsEncryptedFilePrimary() {
    return GetParam() == BookmarkEncryptionStage::kWriteBothReadPreferEncrypted;
  }

 protected:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting();
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(BookmarkModelStorageWithSecondayFileTest,
       SaveSameContentEncryptedAndUnencryptedToDisk) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());

  // Create one local-or-syncable bookmark.
  const BookmarkNode* local_or_syncable_node = model->AddURL(
      model->bookmark_bar_node(), 0, u"Foo", GURL("http://foo.com"));
  task_environment.FastForwardUntilNoTasksRemain();
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName),
      model->client());

  // Create account permanent folders.
  model->CreateAccountPermanentFolders();
  task_environment.FastForwardUntilNoTasksRemain();
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kAccountBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName),
      model->client());

  // Save one account bookmark.
  model->AddURL(model->account_bookmark_bar_node(), 0, u"Bar",
                GURL("http://bar.com"));
  task_environment.FastForwardUntilNoTasksRemain();
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kAccountBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName),
      model->client());

  // Edit the local-or-syncable bookmark.
  model->SetTitle(local_or_syncable_node, u"Foo2",
                  metrics::BookmarkEditSource::kOther);
  task_environment.FastForwardUntilNoTasksRemain();
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName),
      model->client());
}

TEST_P(BookmarkModelStorageWithSecondayFileTest,
       SecondaryFileIsVerifiedOnLoad) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // First create the local-or-syncable and account bookmarks primary and
  // secondary files.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->AddURL(model->bookmark_bar_node(), 0, u"Foo", GURL("http://foo.com"));
  task_environment.FastForwardUntilNoTasksRemain();
  model->CreateAccountPermanentFolders();
  task_environment.FastForwardUntilNoTasksRemain();

  // Reload the model and verify that the secondary files matches the primary
  // files.
  base::HistogramTester histogram_tester;
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  task_environment.FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectUniqueSample(
      "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable.ClearText",
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable.Encrypted",
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.EncryptedBookmarksFileMatchesResult.LocalOrSyncable", true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.BookmarksFileLoadResult.Account.ClearText",
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.BookmarksFileLoadResult.Account.Encrypted",
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.EncryptedBookmarksFileMatchesResult.Account", true,
      /*expected_bucket_count=*/1);
}

TEST_P(BookmarkModelStorageWithSecondayFileTest,
       SecondaryBookmarksFileIsCreatedOnLoadIfMissing) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // First create the local-or-syncable and account bookmarks primary files and
  // delete any secondary files.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->AddURL(model->bookmark_bar_node(), 0, u"Foo", GURL("http://foo.com"));
  task_environment.FastForwardUntilNoTasksRemain();
  model->CreateAccountPermanentFolders();
  task_environment.FastForwardUntilNoTasksRemain();
  // Delete the secondary files.
  ASSERT_TRUE(base::DeleteFile(
      tmp_dir.GetPath().Append(GetLocalOrSyncableSecondaryFilename())));
  ASSERT_TRUE(base::DeleteFile(
      tmp_dir.GetPath().Append(GetAccountSecondaryFilename())));

  // Reload the model and verify that the secondary files is recreated.
  base::HistogramTester histogram_tester;
  model = CreateBookmarkModel();
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  task_environment.FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectUniqueSample(
      GetPrimaryHistogramName(
          "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable"),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      GetSecondaryHistogramName(
          "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable"),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName),
      model->client());

  histogram_tester.ExpectUniqueSample(
      GetPrimaryHistogramName("Bookmarks.BookmarksFileLoadResult.Account"),
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      GetSecondaryHistogramName("Bookmarks.BookmarksFileLoadResult.Account"),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kAccountBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName),
      model->client());

  histogram_tester.ExpectTotalCount(
      base::StrCat({"ImportantFile.WriteDuration",
                    GetSecondaryBookmarkStorageHistogramSuffix()}),
      /*expected_count=*/2);
}

TEST_P(BookmarkModelStorageWithSecondayFileTest,
       SecondaryBookmarksFileIsCreatedOnLoadWithMetadataIfMissing) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  // First create the local-or-syncable and account bookmarks primary files with
  // some sync metadata and delete any secondary files.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto client = std::make_unique<TestBookmarkClientWithFixedSyncMetadata>(
      os_crypt_async_.get(), "local_or_syncable_metadata1",
      "account_metadata1");
  auto model = std::make_unique<BookmarkModel>(std::move(client));
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->AddURL(model->bookmark_bar_node(), 0, u"Foo", GURL("http://foo.com"));
  task_environment.FastForwardUntilNoTasksRemain();
  model->CreateAccountPermanentFolders();
  task_environment.FastForwardUntilNoTasksRemain();
  // Delete the secondary files.
  ASSERT_TRUE(base::DeleteFile(
      tmp_dir.GetPath().Append(GetLocalOrSyncableSecondaryFilename())));
  ASSERT_TRUE(base::DeleteFile(
      tmp_dir.GetPath().Append(GetAccountSecondaryFilename())));

  // Reload the model, force a different sync metadata for the next encoding,
  // verify that the secondary files are recreated and match the primary files.
  base::HistogramTester histogram_tester;
  client = std::make_unique<TestBookmarkClientWithFixedSyncMetadata>(
      os_crypt_async_.get(), "local_or_syncable_metadata2",
      "account_metadata2");
  model = std::make_unique<BookmarkModel>(std::move(client));
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  task_environment.FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectUniqueSample(
      GetPrimaryHistogramName(
          "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable"),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      GetSecondaryHistogramName(
          "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable"),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName),
      model->client());

  histogram_tester.ExpectUniqueSample(
      GetPrimaryHistogramName("Bookmarks.BookmarksFileLoadResult.Account"),
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      GetSecondaryHistogramName("Bookmarks.BookmarksFileLoadResult.Account"),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kAccountBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName),
      model->client());

  histogram_tester.ExpectTotalCount(
      base::StrCat({"ImportantFile.WriteDuration",
                    GetSecondaryBookmarkStorageHistogramSuffix()}),
      /*expected_count=*/2);
}

INSTANTIATE_TEST_SUITE_P(
    BookmarkModelStorageWithSecondayFileTest,
    BookmarkModelStorageWithSecondayFileTest,
    ::testing::Values(BookmarkEncryptionStage::kWriteBothReadOnlyClear,
                      BookmarkEncryptionStage::kWriteBothReadPreferEncrypted));

class BookmarkModelStorageWithEncryptionFileAsPrimaryTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {};

TEST_P(BookmarkModelStorageWithEncryptionFileAsPrimaryTest,
       EncryptedBookmarksFileIsCreatedOnLoadIfMissing) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      os_crypt_async::GetTestOSCryptAsyncForTesting();
  {
    // First create the local-or-syncable and account bookmarks unencrypted
    // files.
    base::test::ScopedFeatureList no_encryption_features;
    test::InitFeaturesForBookmarkTestEncryptionStage(
        no_encryption_features, BookmarkEncryptionStage::kDisabled);
    auto model = std::make_unique<BookmarkModel>(
        std::make_unique<TestBookmarkClient>(os_crypt_async.get()));
    model->Load(tmp_dir.GetPath());
    test::WaitForBookmarkModelToLoad(model.get());
    model->AddURL(model->bookmark_bar_node(), 0, u"Foo",
                  GURL("http://foo.com"));
    task_environment.FastForwardUntilNoTasksRemain();
    model->CreateAccountPermanentFolders();
    task_environment.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(base::PathExists(
        tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName)));
    ASSERT_FALSE(base::PathExists(
        tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName)));
    ASSERT_TRUE(
        base::PathExists(tmp_dir.GetPath().Append(kAccountBookmarksFileName)));
    ASSERT_FALSE(base::PathExists(
        tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName)));
  }

  // Reload the model with the encryption file as primary and verify that the
  // encrypted files is created.
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList encryption_features;
  test::InitFeaturesForBookmarkTestEncryptionStage(encryption_features,
                                                   GetParam());
  auto model = std::make_unique<BookmarkModel>(
      std::make_unique<TestBookmarkClient>(os_crypt_async.get()));
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  task_environment.FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectUniqueSample(
      "Bookmarks.FallbackToClearTextFileOnLoadResult.LocalOrSyncable",
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName),
      model->client());

  histogram_tester.ExpectUniqueSample(
      "Bookmarks.FallbackToClearTextFileOnLoadResult.Account",
      metrics::BookmarksFileLoadResult::kSuccess, /*expected_bucket_count=*/1);
  AssertSameFileContent(
      tmp_dir.GetPath().Append(kAccountBookmarksFileName),
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName),
      model->client());

  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageEncryptedImmediate",
      /*expected_count=*/2);
}

INSTANTIATE_TEST_SUITE_P(
    BookmarkModelStorageWithEncryptionFileAsPrimaryTest,
    BookmarkModelStorageWithEncryptionFileAsPrimaryTest,
    ::testing::Values(
        BookmarkEncryptionStage::kWriteBothReadPreferEncrypted,
        BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted));

TEST(BookmarkModelStorageWithEncryptionWriteOnlyTest,
     ClearTextBookmarksFileIsNeverCreated) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  base::test::ScopedFeatureList encryption_features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      encryption_features,
      BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted);
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      os_crypt_async::GetTestOSCryptAsyncForTesting();

  // First create the local-or-syncable and account bookmarks primary.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto model = std::make_unique<BookmarkModel>(
      std::make_unique<TestBookmarkClient>(os_crypt_async.get()));
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  model->AddURL(model->bookmark_bar_node(), 0, u"Foo", GURL("http://foo.com"));
  task_environment.FastForwardUntilNoTasksRemain();
  model->CreateAccountPermanentFolders();
  task_environment.FastForwardUntilNoTasksRemain();
  // Encrypted files are created when bookmark & account folder are added.
  ASSERT_FALSE(base::PathExists(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName)));
  ASSERT_TRUE(base::PathExists(
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName)));
  ASSERT_FALSE(
      base::PathExists(tmp_dir.GetPath().Append(kAccountBookmarksFileName)));
  ASSERT_TRUE(base::PathExists(
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName)));

  // Reload the model and verify only the encrypted files are read and no clear
  // text files are created.
  base::HistogramTester histogram_tester;
  model = std::make_unique<BookmarkModel>(
      std::make_unique<TestBookmarkClient>(os_crypt_async.get()));
  model->Load(tmp_dir.GetPath());
  test::WaitForBookmarkModelToLoad(model.get());
  task_environment.FastForwardUntilNoTasksRemain();

  // Encrypted files are loaded correctly.
  EXPECT_TRUE(base::PathExists(
      tmp_dir.GetPath().Append(kEncryptedLocalOrSyncableBookmarksFileName)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.GetPath().Append(kEncryptedAccountBookmarksFileName)));
  histogram_tester.ExpectBucketCount(
      "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable.Encrypted",
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Bookmarks.BookmarksFileLoadResult.Account.Encrypted",
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);

  // No clear text files are loaded or created.
  EXPECT_FALSE(base::PathExists(
      tmp_dir.GetPath().Append(kLocalOrSyncableBookmarksFileName)));
  EXPECT_FALSE(
      base::PathExists(tmp_dir.GetPath().Append(kAccountBookmarksFileName)));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.BookmarksFileLoadResult.LocalOrSyncable.ClearText",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Bookmarks.BookmarksFileLoadResult.Account.ClearText",
      /*expected_count=*/0);
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
  ASSERT_EQ(nullptr, model()->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model()->account_other_node());
  ASSERT_EQ(nullptr, model()->account_mobile_node());

  EXPECT_CALL(mock_observer(), BookmarkNodeAdded).Times(3);
  EXPECT_CALL(mock_observer(), OnWillRemoveBookmarks).Times(0);
  EXPECT_CALL(mock_observer(), BookmarkNodeRemoved).Times(0);
  model()->CreateAccountPermanentFolders();

  ASSERT_NE(nullptr, model()->account_bookmark_bar_node());
  ASSERT_NE(nullptr, model()->account_other_node());
  ASSERT_NE(nullptr, model()->account_mobile_node());

  EXPECT_TRUE(model()->account_bookmark_bar_node()->is_permanent_node());
  EXPECT_TRUE(model()->account_other_node()->is_permanent_node());
  EXPECT_TRUE(model()->account_mobile_node()->is_permanent_node());

  EXPECT_EQ(BookmarkNode::BOOKMARK_BAR,
            model()->account_bookmark_bar_node()->type());
  EXPECT_EQ(BookmarkNode::OTHER_NODE, model()->account_other_node()->type());
  EXPECT_EQ(BookmarkNode::MOBILE, model()->account_mobile_node()->type());
}

TEST_F(BookmarkModelTest, RemoveAccountPermanentFolders) {
  model()->CreateAccountPermanentFolders();

  const BookmarkNode* account_bookmark_bar_node =
      model()->account_bookmark_bar_node();
  const BookmarkNode* account_other_node = model()->account_other_node();
  const BookmarkNode* account_mobile_node = model()->account_mobile_node();
  ASSERT_NE(nullptr, account_bookmark_bar_node);
  ASSERT_NE(nullptr, account_other_node);
  ASSERT_NE(nullptr, account_mobile_node);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer(),
              OnWillRemoveBookmarks(_, _, account_mobile_node, _));
  EXPECT_CALL(mock_observer(),
              BookmarkNodeRemoved(_, _, account_mobile_node, _, _));
  EXPECT_CALL(mock_observer(),
              OnWillRemoveBookmarks(_, _, account_other_node, _));
  EXPECT_CALL(mock_observer(),
              BookmarkNodeRemoved(_, _, account_other_node, _, _));
  EXPECT_CALL(mock_observer(),
              OnWillRemoveBookmarks(_, _, account_bookmark_bar_node, _));
  EXPECT_CALL(mock_observer(),
              BookmarkNodeRemoved(_, _, account_bookmark_bar_node, _, _));
  model()->RemoveAccountPermanentFolders();

  EXPECT_EQ(nullptr, model()->account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model()->account_other_node());
  EXPECT_EQ(nullptr, model()->account_mobile_node());
}

TEST_F(BookmarkModelTest, NoOpRemoveAccountPermanentFolders) {
  ASSERT_EQ(nullptr, model()->account_bookmark_bar_node());
  ASSERT_EQ(nullptr, model()->account_other_node());
  ASSERT_EQ(nullptr, model()->account_mobile_node());

  EXPECT_CALL(mock_observer(), OnWillRemoveBookmarks).Times(0);
  EXPECT_CALL(mock_observer(), BookmarkNodeRemoved).Times(0);
  model()->RemoveAccountPermanentFolders();

  EXPECT_EQ(nullptr, model()->account_bookmark_bar_node());
  EXPECT_EQ(nullptr, model()->account_other_node());
  EXPECT_EQ(nullptr, model()->account_mobile_node());
}

TEST_F(BookmarkModelTest, IsLocalOnlyNodeWithSyncFeatureOff) {
  static_cast<TestBookmarkClient*>(model()->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(false);

  model()->CreateAccountPermanentFolders();

  ASSERT_NE(nullptr, model()->bookmark_bar_node());
  ASSERT_NE(nullptr, model()->other_node());
  ASSERT_NE(nullptr, model()->mobile_node());

  const BookmarkNode* local_folder =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"local_folder");
  const BookmarkNode* account_folder = model()->AddFolder(
      model()->account_bookmark_bar_node(), 0, u"account_folder");

  EXPECT_TRUE(model()->IsLocalOnlyNode(*model()->root_node()));
  EXPECT_TRUE(model()->IsLocalOnlyNode(*model()->bookmark_bar_node()));
  EXPECT_TRUE(model()->IsLocalOnlyNode(*model()->other_node()));
  EXPECT_TRUE(model()->IsLocalOnlyNode(*model()->mobile_node()));
  EXPECT_TRUE(model()->IsLocalOnlyNode(*local_folder));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*model()->account_bookmark_bar_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*model()->account_other_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*model()->account_mobile_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*account_folder));
}

TEST_F(BookmarkModelTest, IsLocalOnlyNodeWithSyncFeatureOn) {
  static_cast<TestBookmarkClient*>(model()->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  const BookmarkNode* folder =
      model()->AddFolder(model()->bookmark_bar_node(), 0, u"local_folder");

  EXPECT_TRUE(model()->IsLocalOnlyNode(*model()->root_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*model()->bookmark_bar_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*model()->other_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*model()->mobile_node()));
  EXPECT_FALSE(model()->IsLocalOnlyNode(*folder));
}

TEST_F(BookmarkModelTest, IsLocalOnlyNodeWithSyncFeatureOnAndDettachedNode) {
  static_cast<TestBookmarkClient*>(model()->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  auto dettached_node = std::make_unique<BookmarkNode>(
      /*id=*/200, base::Uuid::GenerateRandomV4(), GURL());

  EXPECT_TRUE(model()->IsLocalOnlyNode(*dettached_node));
}

TEST_F(BookmarkModelTest, UserFolderDepthHistograms) {
  constexpr char kUrlAddedMetricName[] = "Bookmarks.UserFolderDepth.UrlAdded";
  constexpr char kUrlOpenedMetricName[] = "Bookmarks.UserFolderDepth.UrlOpened";

  TestNode bbn;
  PopulateNodeFromString("[ ] [ [ ] [ ] ]", &bbn);
  const BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  PopulateBookmarkNode(&bbn, model(), bookmark_bar);

  auto* folder_0 = bookmark_bar->children()[0].get();
  auto* folder_1 = bookmark_bar->children()[1].get();
  auto* folder_1_0 = folder_1->children()[0].get();
  auto* folder_1_1 = folder_1->children()[1].get();

  auto* url_0 =
      model()->AddNewURL(bookmark_bar, 0, u"url_0", GURL("http://url_0"));

  histogram_tester()->ExpectTotalCount(kUrlAddedMetricName, 1);
  histogram_tester()->ExpectBucketCount(kUrlAddedMetricName,
                                        /*sample=*/0, /*expected_count=*/1);

  auto* url_1 = model()->AddNewURL(folder_0, 0, u"url_1", GURL("http://url_1"));
  histogram_tester()->ExpectTotalCount(kUrlAddedMetricName, 2);
  histogram_tester()->ExpectBucketCount(kUrlAddedMetricName,
                                        /*sample=*/1, /*expected_count=*/1);

  auto* url_2 =
      model()->AddNewURL(folder_1_0, 0, u"url_2", GURL("http://url_2"));
  auto* url_3 =
      model()->AddNewURL(folder_1_1, 0, u"url_3", GURL("http://url_3"));
  model()->AddNewURL(folder_1_1, 0, u"url_4", GURL("http://url_4"));
  histogram_tester()->ExpectTotalCount(kUrlAddedMetricName, 5);
  histogram_tester()->ExpectBucketCount(kUrlAddedMetricName,
                                        /*sample=*/2, /*expected_count=*/3);

  model()->UpdateLastUsedTime(url_0, base::Time::Now(), /*just_opened=*/true);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 1);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/0, /*expected_count=*/1);

  model()->UpdateLastUsedTime(url_1, base::Time::Now(), /*just_opened=*/true);
  model()->UpdateLastUsedTime(url_1, base::Time::Now(), /*just_opened=*/true);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 3);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/1, /*expected_count=*/2);

  model()->UpdateLastUsedTime(url_2, base::Time::Now(), /*just_opened=*/true);
  model()->UpdateLastUsedTime(url_3, base::Time::Now(), /*just_opened=*/true);
  model()->UpdateLastUsedTime(url_3, base::Time::Now(), /*just_opened=*/true);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 6);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/2, /*expected_count=*/3);

  // This update isn't a result of an open, but rather a sync event.
  // The histogram count should remain the same.
  model()->UpdateLastUsedTime(url_3, base::Time::Now(), /*just_opened=*/false);
  histogram_tester()->ExpectTotalCount(kUrlOpenedMetricName, 6);
  histogram_tester()->ExpectBucketCount(kUrlOpenedMetricName,
                                        /*sample=*/2, /*expected_count=*/3);
}

// TODO(crbug.com/395071423): remove this test (split into smaller ones)
TEST_F(BookmarkModelTest, IsVisible) {
  // Test with empty local folders only.
  EXPECT_TRUE(model()->root_node()->IsVisible());

  // Check per-platform visibility of empty permanent nodes.
  if (TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_TRUE(model()->bookmark_bar_node()->IsVisible());
    EXPECT_TRUE(model()->other_node()->IsVisible());
    EXPECT_FALSE(model()->mobile_node()->IsVisible());
  } else {
    EXPECT_FALSE(model()->bookmark_bar_node()->IsVisible());
    EXPECT_FALSE(model()->other_node()->IsVisible());
    EXPECT_TRUE(model()->mobile_node()->IsVisible());
  }
  // Create empty account folders.
  model()->CreateAccountPermanentFolders();

  // All empty local permanent folders are now hidden.
  // The account permanent folders are visible, except for the mobile node.
  EXPECT_TRUE(model()->root_node()->IsVisible());
  EXPECT_FALSE(model()->bookmark_bar_node()->IsVisible());
  EXPECT_FALSE(model()->other_node()->IsVisible());

  if (TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_FALSE(model()->mobile_node()->IsVisible());
  } else {
    EXPECT_TRUE(model()->mobile_node()->IsVisible());
  }

  // Check per-platform visibility of empty account permanent nodes.
  if (TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_TRUE(model()->account_bookmark_bar_node()->IsVisible());
    EXPECT_TRUE(model()->account_other_node()->IsVisible());
    EXPECT_FALSE(model()->account_mobile_node()->IsVisible());
  } else {
    EXPECT_FALSE(model()->account_bookmark_bar_node()->IsVisible());
    EXPECT_FALSE(model()->account_other_node()->IsVisible());
    EXPECT_TRUE(model()->account_mobile_node()->IsVisible());
  }

  // Make the local bookmark bar node non-empty. Nodes that were previously
  // hidden because there were no local bookmarks are now visible.
  model()->AddURL(model()->bookmark_bar_node(), 0, u"Chromium",
                  GURL("http://www.chromium.org"));
  EXPECT_TRUE(model()->bookmark_bar_node()->IsVisible());
  if (TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    // On desktop, the other node was previously hidden and is now visible.
    EXPECT_TRUE(model()->other_node()->IsVisible());
  } else {
    // On mobile, the other node is always hidden when empty and the mobile node
    // is always visible (so there is no change as a result of the bookmark bar
    // becoming non-empty).
    EXPECT_FALSE(model()->other_node()->IsVisible());
    EXPECT_TRUE(model()->mobile_node()->IsVisible());
  }

  // Make the account mobile node folder non-empty. It is now visible.
  model()->AddURL(model()->account_mobile_node(), 0, u"Chromium",
                  GURL("http://www.chromium.org"));
  EXPECT_TRUE(model()->account_mobile_node()->IsVisible());
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
    return std::ranges::contains(updated_nodes_, node);
  }

  BookmarkModel* model() { return model_.get(); }

  void ClearUpdatedNodes() { updated_nodes_.clear(); }
  size_t updated_node_count() const { return updated_nodes_.size(); }

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

 private:
  std::unique_ptr<BookmarkModel> model_;
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> updated_nodes_;
};

// Test that BookmarkModel::OnFaviconsChanged() sends a notification that the
// favicon changed to each BookmarkNode which has either a matching page URL
// (e.g. http://www.google.com) or a matching icon URL
// (e.g. http://www.google.com/favicon.ico).
TEST_F(BookmarkModelFaviconTest, FaviconsChangedObserver) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const std::u16string kTitle(u"foo");
  const GURL kPageURL1("http://www.google.com");
  const GURL kPageURL2("http://www.google.ca");
  const GURL kPageURL3("http://www.amazon.com");
  const GURL kFaviconURL12("http://www.google.com/favicon.ico");
  const GURL kFaviconURL3("http://www.amazon.com/favicon.ico");

  const BookmarkNode* node1 =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kPageURL1);
  const BookmarkNode* node2 =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kPageURL2);
  const BookmarkNode* node3 =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kPageURL3);
  const BookmarkNode* node4 =
      model()->AddURL(bookmark_bar_node, 0, kTitle, kPageURL3);

  {
    OnFaviconLoaded(AsMutable(node1), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node2), kFaviconURL12);
    OnFaviconLoaded(AsMutable(node3), kFaviconURL3);
    OnFaviconLoaded(AsMutable(node4), kFaviconURL3);

    ClearUpdatedNodes();
    std::set<GURL> changed_page_urls;
    changed_page_urls.insert(kPageURL2);
    changed_page_urls.insert(kPageURL3);
    model()->OnFaviconsChanged(changed_page_urls, GURL());
    ASSERT_EQ(3u, updated_node_count());
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
    model()->OnFaviconsChanged(std::set<GURL>(), kFaviconURL12);
    ASSERT_EQ(2u, updated_node_count());
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
    model()->OnFaviconsChanged(changed_page_urls, kFaviconURL12);
    ASSERT_EQ(2u, updated_node_count());
    EXPECT_TRUE(WasNodeUpdated(node1));
    EXPECT_TRUE(WasNodeUpdated(node2));
  }
}

TEST_F(BookmarkModelFaviconTest, ShouldResetFaviconStatusAfterRestore) {
  const std::u16string kTitle(u"foo");
  const GURL kPageURL("http://www.google.com");

  const BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  const BookmarkNode* node = model()->AddURL(bookmark_bar, 0, kTitle, kPageURL);

  ASSERT_FALSE(node->is_favicon_loaded());
  ASSERT_FALSE(node->is_favicon_loading());

  // Initiate favicon loading.
  model()->GetFavicon(node);
  ASSERT_TRUE(node->is_favicon_loading());

  model()->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);

  ASSERT_TRUE(static_cast<TestBookmarkClientWithUndo*>(model()->client())
                  ->RestoreLastRemovedBookmark(model()));

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
