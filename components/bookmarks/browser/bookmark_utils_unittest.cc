// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_utils.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using base::ASCIIToUTF16;
using std::string;

namespace bookmarks {
namespace {

class BookmarkUtilsTest : public testing::Test,
                          public BaseBookmarkModelObserver {
 public:
  BookmarkUtilsTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        grouped_changes_beginning_count_(0),
        grouped_changes_ended_count_(0) {}

  ~BookmarkUtilsTest() override {}

// Copy and paste is not yet supported on iOS. http://crbug.com/228147
#if !defined(OS_IOS)
  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }
#endif  // !defined(OS_IOS)

  // Certain user actions require multiple changes to the bookmark model,
  // however these modifications need to be atomic for the undo framework. The
  // BaseBookmarkModelObserver is used to inform the boundaries of the user
  // action. For example, when multiple bookmarks are cut to the clipboard we
  // expect one call each to GroupedBookmarkChangesBeginning/Ended.
  void ExpectGroupedChangeCount(int expected_beginning_count,
                                int expected_ended_count) {
    // The undo framework is not used under Android.  Thus the group change
    // events will not be fired and so should not be tested for Android.
#if !defined(OS_ANDROID)
    EXPECT_EQ(grouped_changes_beginning_count_, expected_beginning_count);
    EXPECT_EQ(grouped_changes_ended_count_, expected_ended_count);
#endif
  }

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}

  void GroupedBookmarkChangesBeginning(BookmarkModel* model) override {
    ++grouped_changes_beginning_count_;
  }

  void GroupedBookmarkChangesEnded(BookmarkModel* model) override {
    ++grouped_changes_ended_count_;
  }

  // Clipboard requires a full TaskEnvironment.
  base::test::TaskEnvironment task_environment_;

  int grouped_changes_beginning_count_;
  int grouped_changes_ended_count_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkUtilsTest);
};

TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesWordPhraseQuery) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("foo bar"),
                                            GURL("http://www.google.com"));
  const BookmarkNode* node2 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("baz buz"),
                                            GURL("http://www.cnn.com"));
  const BookmarkNode* folder1 =
      model->AddFolder(model->other_node(), 0, ASCIIToUTF16("foo"));
  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.word_phrase_query.reset(new base::string16);
  // No nodes are returned for empty string.
  *query.word_phrase_query = ASCIIToUTF16("");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  EXPECT_TRUE(nodes.empty());
  nodes.clear();

  // No nodes are returned for space-only string.
  *query.word_phrase_query = ASCIIToUTF16("   ");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  EXPECT_TRUE(nodes.empty());
  nodes.clear();

  // Node "foo bar" and folder "foo" are returned in search results.
  *query.word_phrase_query = ASCIIToUTF16("foo");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(2U, nodes.size());
  EXPECT_TRUE(nodes[0] == folder1);
  EXPECT_TRUE(nodes[1] == node1);
  nodes.clear();

  // Ensure url matches return in search results.
  *query.word_phrase_query = ASCIIToUTF16("cnn");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node2);
  nodes.clear();

  // Ensure folder "foo" is not returned in more specific search.
  *query.word_phrase_query = ASCIIToUTF16("foo bar");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  // Bookmark Bar and Other Bookmarks are not returned in search results.
  *query.word_phrase_query = ASCIIToUTF16("Bookmark");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();
}

// Check exact matching against a URL query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesUrl) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(),
                0,
                ASCIIToUTF16("Google Calendar"),
                GURL("https://www.google.com/calendar"));

  model->AddFolder(model->other_node(), 0, ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.url.reset(new base::string16);
  *query.url = ASCIIToUTF16("https://www.google.com/");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  *query.url = ASCIIToUTF16("calendar");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();

  // Empty URL should not match folders.
  *query.url = ASCIIToUTF16("");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();
}

// Check exact matching against a title query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesTitle) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(),
                0,
                ASCIIToUTF16("Google Calendar"),
                GURL("https://www.google.com/calendar"));

  const BookmarkNode* folder1 =
      model->AddFolder(model->other_node(), 0, ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.title.reset(new base::string16);
  *query.title = ASCIIToUTF16("Google");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  *query.title = ASCIIToUTF16("Calendar");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();

  // Title should match folders.
  *query.title = ASCIIToUTF16("Folder");
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == folder1);
  nodes.clear();
}

// Check matching against a query with multiple predicates.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesConjunction) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(),
                0,
                ASCIIToUTF16("Google Calendar"),
                GURL("https://www.google.com/calendar"));

  model->AddFolder(model->other_node(), 0, ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;

  // Test all fields matching.
  query.word_phrase_query.reset(new base::string16(ASCIIToUTF16("www")));
  query.url.reset(new base::string16(ASCIIToUTF16("https://www.google.com/")));
  query.title.reset(new base::string16(ASCIIToUTF16("Google")));
  GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  std::unique_ptr<base::string16>* fields[] = {&query.word_phrase_query,
                                               &query.url, &query.title};

  // Test two fields matching.
  for (size_t i = 0; i < base::size(fields); i++) {
    std::unique_ptr<base::string16> original_value(fields[i]->release());
    GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
    ASSERT_EQ(1U, nodes.size());
    EXPECT_TRUE(nodes[0] == node1);
    nodes.clear();
    *fields[i] = std::move(original_value);
  }

  // Test two fields matching with one non-matching field.
  for (size_t i = 0; i < base::size(fields); i++) {
    std::unique_ptr<base::string16> original_value(fields[i]->release());
    fields[i]->reset(new base::string16(ASCIIToUTF16("fjdkslafjkldsa")));
    GetBookmarksMatchingProperties(model.get(), query, 100, &nodes);
    ASSERT_EQ(0U, nodes.size());
    nodes.clear();
    *fields[i] = std::move(original_value);
  }
}

// Copy and paste is not yet supported on iOS. http://crbug.com/228147
#if !defined(OS_IOS)
TEST_F(BookmarkUtilsTest, DISABLED_PasteBookmarkFromURL) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const base::string16 url_text = ASCIIToUTF16("http://www.google.com/");
  const BookmarkNode* new_folder = model->AddFolder(
      model->bookmark_bar_node(), 0, ASCIIToUTF16("New_Folder"));

  // Write blank text to clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(base::string16());
  }
  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(CanPasteFromClipboard(model.get(), new_folder));

  // Write some valid url to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(url_text);
  }
  // Now we should be able to paste from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), new_folder));

  PasteFromClipboard(model.get(), new_folder, 0);
  ASSERT_EQ(1u, new_folder->children().size());

  // Url for added node should be same as url_text.
  EXPECT_EQ(url_text,
            ASCIIToUTF16(new_folder->children().front()->url().spec()));
}

// TODO(https://crbug.com/1010182): Fix flakes and re-enable this test.
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_CopyPaste DISABLED_CopyPaste
#else
#define MAYBE_CopyPaste CopyPaste
#endif
TEST_F(BookmarkUtilsTest, MAYBE_CopyPaste) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), model->bookmark_bar_node()));

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(CanPasteFromClipboard(model.get(), model->bookmark_bar_node()));
}

// Test for updating title such that url and title pair are unique among the
// children of parent.
TEST_F(BookmarkUtilsTest, DISABLED_MakeTitleUnique) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const base::string16 url_text = ASCIIToUTF16("http://www.google.com/");
  const base::string16 title_text = ASCIIToUTF16("foobar");
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();

  const BookmarkNode* node =
      model->AddURL(bookmark_bar_node, 0, title_text, GURL(url_text));

  EXPECT_EQ(url_text,
            ASCIIToUTF16(bookmark_bar_node->children()[0]->url().spec()));
  EXPECT_EQ(title_text, bookmark_bar_node->children()[0]->GetTitle());

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // Now we should be able to paste from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), bookmark_bar_node));

  PasteFromClipboard(model.get(), bookmark_bar_node, 1);
  ASSERT_EQ(2u, bookmark_bar_node->children().size());

  // Url for added node should be same as url_text.
  EXPECT_EQ(url_text,
            ASCIIToUTF16(bookmark_bar_node->children()[1]->url().spec()));
  // Title for added node should be numeric subscript suffix with copied node
  // title.
  EXPECT_EQ(ASCIIToUTF16("foobar (1)"),
            bookmark_bar_node->children()[1]->GetTitle());
}

TEST_F(BookmarkUtilsTest, DISABLED_CopyPasteMetaInfo) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "somekey", "somevalue");
  model->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // Paste node to a different folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, ASCIIToUTF16("Folder"));
  EXPECT_EQ(0u, folder->children().size());

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), folder));

  PasteFromClipboard(model.get(), folder, 0);
  ASSERT_EQ(1u, folder->children().size());

  // Verify that the pasted node contains the same meta info.
  const BookmarkNode* pasted = folder->children().front().get();
  ASSERT_TRUE(pasted->GetMetaInfoMap());
  EXPECT_EQ(2u, pasted->GetMetaInfoMap()->size());
  std::string value;
  EXPECT_TRUE(pasted->GetMetaInfo("somekey", &value));
  EXPECT_EQ("somevalue", value);
  EXPECT_TRUE(pasted->GetMetaInfo("someotherkey", &value));
  EXPECT_EQ("someothervalue", value);
}

#if defined(OS_LINUX) || defined(OS_MACOSX)
// http://crbug.com/396472
#define MAYBE_CutToClipboard DISABLED_CutToClipboard
#else
#define MAYBE_CutToClipboard CutToClipboard
#endif
TEST_F(BookmarkUtilsTest, MAYBE_CutToClipboard) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->AddObserver(this);

  base::string16 title(ASCIIToUTF16("foo"));
  GURL url("http://foo.com");
  const BookmarkNode* n1 = model->AddURL(model->other_node(), 0, title, url);
  const BookmarkNode* n2 = model->AddURL(model->other_node(), 1, title, url);

  // Cut the nodes to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(n1);
  nodes.push_back(n2);
  CopyToClipboard(model.get(), nodes, true);

  // Make sure the nodes were removed.
  EXPECT_EQ(0u, model->other_node()->children().size());

  // Make sure observers were notified the set of changes should be grouped.
  ExpectGroupedChangeCount(1, 1);

  // And make sure we can paste from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), model->other_node()));
}

TEST_F(BookmarkUtilsTest, PasteNonEditableNodes) {
  // Load a model with an managed node that is not editable.
  std::unique_ptr<TestBookmarkClient> client(new TestBookmarkClient());
  auto owned_managed_node =
      std::make_unique<BookmarkPermanentNode>(100, BookmarkNode::FOLDER);
  BookmarkPermanentNode* managed_node = owned_managed_node.get();
  client->SetManagedNodeToLoad(std::move(owned_managed_node));

  std::unique_ptr<BookmarkModel> model(
      TestBookmarkClient::CreateModelWithClient(std::move(client)));
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), model->bookmark_bar_node()));

  // But it can't be pasted into a non-editable folder.
  BookmarkClient* upcast = model->client();
  EXPECT_FALSE(upcast->CanBeEditedByUser(managed_node));
  EXPECT_FALSE(CanPasteFromClipboard(model.get(), managed_node));
}
#endif  // !defined(OS_IOS)

TEST_F(BookmarkUtilsTest, GetParentForNewNodes) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  // This tests the case where selection contains one item and that item is a
  // folder.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model->bookmark_bar_node());
  size_t index = size_t{-1};
  const BookmarkNode* real_parent =
      GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(0u, index);

  nodes.clear();

  // This tests the case where selection contains one item and that item is an
  // url.
  const BookmarkNode* page1 = model->AddURL(model->bookmark_bar_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("http://google.com"));
  nodes.push_back(page1);
  real_parent = GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(1u, index);

  // This tests the case where selection has more than one item.
  const BookmarkNode* folder1 =
      model->AddFolder(model->bookmark_bar_node(), 1, ASCIIToUTF16("Folder 1"));
  nodes.push_back(folder1);
  real_parent = GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(2u, index);

  // This tests the case where selection doesn't contain any items.
  nodes.clear();
  real_parent = GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(2u, index);
}

// Verifies that meta info is copied when nodes are cloned.
TEST_F(BookmarkUtilsTest, CloneMetaInfo) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  // Add a node containing meta info.
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "somekey", "somevalue");
  model->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Clone node to a different folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, ASCIIToUTF16("Folder"));
  std::vector<BookmarkNodeData::Element> elements;
  BookmarkNodeData::Element node_data(node);
  elements.push_back(node_data);
  EXPECT_EQ(0u, folder->children().size());
  CloneBookmarkNode(model.get(), elements, folder, 0, false);
  ASSERT_EQ(1u, folder->children().size());

  // Verify that the cloned node contains the same meta info.
  const BookmarkNode* clone = folder->children().front().get();
  ASSERT_TRUE(clone->GetMetaInfoMap());
  EXPECT_EQ(2u, clone->GetMetaInfoMap()->size());
  std::string value;
  EXPECT_TRUE(clone->GetMetaInfo("somekey", &value));
  EXPECT_EQ("somevalue", value);
  EXPECT_TRUE(clone->GetMetaInfo("someotherkey", &value));
  EXPECT_EQ("someothervalue", value);
}

// Verifies that meta info fields in the non cloned set are not copied when
// cloning a bookmark.
TEST_F(BookmarkUtilsTest, CloneBookmarkResetsNonClonedKey) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->AddNonClonedKey("foo");
  const BookmarkNode* parent = model->other_node();
  const BookmarkNode* node = model->AddURL(
      parent, 0, ASCIIToUTF16("title"), GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "foo", "ignored value");
  model->SetNodeMetaInfo(node, "bar", "kept value");
  std::vector<BookmarkNodeData::Element> elements;
  BookmarkNodeData::Element node_data(node);
  elements.push_back(node_data);

  // Cloning a bookmark should clear the non cloned key.
  CloneBookmarkNode(model.get(), elements, parent, 0, true);
  ASSERT_EQ(2u, parent->children().size());
  std::string value;
  EXPECT_FALSE(parent->children().front()->GetMetaInfo("foo", &value));

  // Other keys should still be cloned.
  EXPECT_TRUE(parent->children().front()->GetMetaInfo("bar", &value));
  EXPECT_EQ("kept value", value);
}

// Verifies that meta info fields in the non cloned set are not copied when
// cloning a folder.
TEST_F(BookmarkUtilsTest, CloneFolderResetsNonClonedKey) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->AddNonClonedKey("foo");
  const BookmarkNode* parent = model->other_node();
  const BookmarkNode* node = model->AddFolder(parent, 0, ASCIIToUTF16("title"));
  model->SetNodeMetaInfo(node, "foo", "ignored value");
  model->SetNodeMetaInfo(node, "bar", "kept value");
  std::vector<BookmarkNodeData::Element> elements;
  BookmarkNodeData::Element node_data(node);
  elements.push_back(node_data);

  // Cloning a folder should clear the non cloned key.
  CloneBookmarkNode(model.get(), elements, parent, 0, true);
  ASSERT_EQ(2u, parent->children().size());
  std::string value;
  EXPECT_FALSE(parent->children().front()->GetMetaInfo("foo", &value));

  // Other keys should still be cloned.
  EXPECT_TRUE(parent->children().front()->GetMetaInfo("bar", &value));
  EXPECT_EQ("kept value", value);
}

TEST_F(BookmarkUtilsTest, RemoveAllBookmarks) {
  // Load a model with an managed node that is not editable.
  std::unique_ptr<TestBookmarkClient> client(new TestBookmarkClient());
  auto owned_managed_node =
      std::make_unique<BookmarkPermanentNode>(100, BookmarkNode::FOLDER);
  BookmarkPermanentNode* managed_node = owned_managed_node.get();
  client->SetManagedNodeToLoad(std::move(owned_managed_node));

  std::unique_ptr<BookmarkModel> model(
      TestBookmarkClient::CreateModelWithClient(std::move(client)));
  EXPECT_TRUE(model->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model->other_node()->children().empty());
  EXPECT_TRUE(model->mobile_node()->children().empty());
  EXPECT_TRUE(managed_node->children().empty());

  const base::string16 title = base::ASCIIToUTF16("Title");
  const GURL url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, title, url);
  model->AddURL(model->other_node(), 0, title, url);
  model->AddURL(model->mobile_node(), 0, title, url);
  model->AddURL(managed_node, 0, title, url);

  std::vector<const BookmarkNode*> nodes;
  model->GetNodesByURL(url, &nodes);
  ASSERT_EQ(4u, nodes.size());

  RemoveAllBookmarks(model.get(), url);

  nodes.clear();
  model->GetNodesByURL(url, &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_TRUE(model->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model->other_node()->children().empty());
  EXPECT_TRUE(model->mobile_node()->children().empty());
  EXPECT_EQ(1u, managed_node->children().size());
}

}  // namespace
}  // namespace bookmarks
