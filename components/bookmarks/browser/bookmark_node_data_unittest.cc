// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace bookmarks {

class BookmarkNodeDataTest : public testing::Test {
 public:
  BookmarkNodeDataTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    model_ = TestBookmarkClient::CreateModel();
    test::WaitForBookmarkModelToLoad(model_.get());
    bool success = profile_dir_.CreateUniqueTempDir();
    ASSERT_TRUE(success);
  }

  void TearDown() override {
    model_.reset();
    bool success = profile_dir_.Delete();
    ASSERT_TRUE(success);
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  const base::FilePath& GetProfilePath() const {
    return profile_dir_.GetPath();
  }

  BookmarkModel* model() { return model_.get(); }

 protected:
  ui::Clipboard& clipboard() { return *ui::Clipboard::GetForCurrentThread(); }

 private:
  base::ScopedTempDir profile_dir_;
  std::unique_ptr<BookmarkModel> model_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeDataTest);
};

namespace {

std::unique_ptr<ui::OSExchangeData::Provider> CloneProvider(
    const ui::OSExchangeData& data) {
  return data.provider().Clone();
}

}  // namespace

// Makes sure BookmarkNodeData is initially invalid.
TEST_F(BookmarkNodeDataTest, InitialState) {
  BookmarkNodeData data;
  EXPECT_FALSE(data.is_valid());
}

// Makes sure reading bogus data leaves the BookmarkNodeData invalid.
TEST_F(BookmarkNodeDataTest, BogusRead) {
  ui::OSExchangeData data;
  BookmarkNodeData drag_data;
  EXPECT_FALSE(drag_data.Read(ui::OSExchangeData(CloneProvider(data))));
  EXPECT_FALSE(drag_data.is_valid());
}

// Writes a URL to the clipboard and make sure BookmarkNodeData can correctly
// read it.
TEST_F(BookmarkNodeDataTest, JustURL) {
  const GURL url("http://google.com");
  const base::string16 title(ASCIIToUTF16("google.com"));

  ui::OSExchangeData data;
  data.SetURL(url, title);

  BookmarkNodeData drag_data;
  EXPECT_TRUE(drag_data.Read(ui::OSExchangeData(CloneProvider(data))));
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1u, drag_data.size());
  EXPECT_TRUE(drag_data.elements[0].is_url);
  EXPECT_EQ(url, drag_data.elements[0].url);
  EXPECT_EQ(title, drag_data.elements[0].title);
  EXPECT_TRUE(drag_data.elements[0].date_added.is_null());
  EXPECT_TRUE(drag_data.elements[0].date_folder_modified.is_null());
  EXPECT_EQ(0u, drag_data.elements[0].children.size());
}

TEST_F(BookmarkNodeDataTest, URL) {
  // Write a single node representing a URL to the clipboard.
  const BookmarkNode* root = model()->bookmark_bar_node();
  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("foo.com"));
  const BookmarkNode* node = model()->AddURL(root, 0, title, url);
  BookmarkNodeData drag_data(node);
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1u, drag_data.size());
  EXPECT_TRUE(drag_data.elements[0].is_url);
  EXPECT_EQ(url, drag_data.elements[0].url);
  EXPECT_EQ(title, drag_data.elements[0].title);
  EXPECT_EQ(node->date_added(), drag_data.elements[0].date_added);
  EXPECT_EQ(node->date_folder_modified(),
            drag_data.elements[0].date_folder_modified);
  ui::OSExchangeData data;
  drag_data.Write(GetProfilePath(), &data);

  // Now read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1u, read_data.size());
  EXPECT_TRUE(read_data.elements[0].is_url);
  EXPECT_EQ(url, read_data.elements[0].url);
  EXPECT_EQ(title, read_data.elements[0].title);
  EXPECT_TRUE(read_data.elements[0].date_added.is_null());
  EXPECT_TRUE(read_data.elements[0].date_folder_modified.is_null());
  EXPECT_TRUE(read_data.GetFirstNode(model(), GetProfilePath()) == node);

  // Make sure asking for the node with a different profile returns NULL.
  base::ScopedTempDir other_profile_dir;
  EXPECT_TRUE(other_profile_dir.CreateUniqueTempDir());
  EXPECT_TRUE(read_data.GetFirstNode(model(), other_profile_dir.GetPath()) ==
              nullptr);

  // Writing should also put the URL and title on the clipboard.
  GURL read_url;
  base::string16 read_title;
  EXPECT_TRUE(data2.GetURLAndTitle(
      ui::OSExchangeData::CONVERT_FILENAMES, &read_url, &read_title));
  EXPECT_EQ(url, read_url);
  EXPECT_EQ(title, read_title);
}

// Tests writing a folder to the clipboard.
TEST_F(BookmarkNodeDataTest, Folder) {
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* g1 = model()->AddFolder(root, 0, ASCIIToUTF16("g1"));
  model()->AddFolder(g1, 0, ASCIIToUTF16("g11"));
  const BookmarkNode* g12 = model()->AddFolder(g1, 0, ASCIIToUTF16("g12"));

  BookmarkNodeData drag_data(g12);
  EXPECT_TRUE(drag_data.is_valid());
  ASSERT_EQ(1u, drag_data.size());
  EXPECT_EQ(g12->GetTitle(), drag_data.elements[0].title);
  EXPECT_FALSE(drag_data.elements[0].is_url);
  EXPECT_EQ(g12->date_added(), drag_data.elements[0].date_added);
  EXPECT_EQ(g12->date_folder_modified(),
            drag_data.elements[0].date_folder_modified);

  ui::OSExchangeData data;
  drag_data.Write(GetProfilePath(), &data);

  // Now read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1u, read_data.size());
  EXPECT_EQ(g12->GetTitle(), read_data.elements[0].title);
  EXPECT_FALSE(read_data.elements[0].is_url);
  EXPECT_TRUE(read_data.elements[0].date_added.is_null());
  EXPECT_TRUE(read_data.elements[0].date_folder_modified.is_null());

  // We should get back the same node when asking for the same profile.
  const BookmarkNode* r_g12 = read_data.GetFirstNode(model(), GetProfilePath());
  EXPECT_TRUE(g12 == r_g12);

  // A different profile should return NULL for the node.
  base::ScopedTempDir other_profile_dir;
  EXPECT_TRUE(other_profile_dir.CreateUniqueTempDir());
  EXPECT_TRUE(read_data.GetFirstNode(model(), other_profile_dir.GetPath()) ==
              nullptr);
}

// Tests reading/writing a folder with children.
TEST_F(BookmarkNodeDataTest, FolderWithChild) {
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, ASCIIToUTF16("g1"));

  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("blah2"));

  model()->AddURL(folder, 0, title, url);

  BookmarkNodeData drag_data(folder);

  ui::OSExchangeData data;
  drag_data.Write(GetProfilePath(), &data);

  // Now read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  ASSERT_EQ(1u, read_data.size());
  ASSERT_EQ(1u, read_data.elements[0].children.size());
  const BookmarkNodeData::Element& read_child =
      read_data.elements[0].children[0];

  EXPECT_TRUE(read_child.is_url);
  EXPECT_EQ(title, read_child.title);
  EXPECT_EQ(url, read_child.url);
  EXPECT_TRUE(read_data.elements[0].date_added.is_null());
  EXPECT_TRUE(read_data.elements[0].date_folder_modified.is_null());
  EXPECT_TRUE(read_child.is_url);

  // And make sure we get the node back.
  const BookmarkNode* r_folder =
      read_data.GetFirstNode(model(), GetProfilePath());
  EXPECT_TRUE(folder == r_folder);
}

// Tests reading/writing of multiple nodes.
TEST_F(BookmarkNodeDataTest, MultipleNodes) {
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, ASCIIToUTF16("g1"));

  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("blah2"));

  const BookmarkNode* url_node = model()->AddURL(folder, 0, title, url);

  // Write the nodes to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(folder);
  nodes.push_back(url_node);
  BookmarkNodeData drag_data(nodes);
  ui::OSExchangeData data;
  drag_data.Write(GetProfilePath(), &data);

  // Read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(2u, read_data.size());
  ASSERT_EQ(1u, read_data.elements[0].children.size());
  EXPECT_TRUE(read_data.elements[0].date_added.is_null());
  EXPECT_TRUE(read_data.elements[0].date_folder_modified.is_null());

  const BookmarkNodeData::Element& read_folder = read_data.elements[0];
  EXPECT_FALSE(read_folder.is_url);
  EXPECT_EQ(ASCIIToUTF16("g1"), read_folder.title);
  EXPECT_EQ(1u, read_folder.children.size());

  const BookmarkNodeData::Element& read_url = read_data.elements[1];
  EXPECT_TRUE(read_url.is_url);
  EXPECT_EQ(title, read_url.title);
  EXPECT_EQ(0u, read_url.children.size());

  // And make sure we get the node back.
  std::vector<const BookmarkNode*> read_nodes =
      read_data.GetNodes(model(), GetProfilePath());
  ASSERT_EQ(2u, read_nodes.size());
  EXPECT_TRUE(read_nodes[0] == folder);
  EXPECT_TRUE(read_nodes[1] == url_node);

  // Asking for the first node should return NULL with more than one element
  // present.
  EXPECT_TRUE(read_data.GetFirstNode(model(), GetProfilePath()) == nullptr);
}

TEST_F(BookmarkNodeDataTest, DISABLED_WriteToClipboardURL) {
  BookmarkNodeData data;
  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("blah"));

  data.ReadFromTuple(url, title);
  data.WriteToClipboard();

  // Now read the data back in.
  base::string16 clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste, &clipboard_result);
  EXPECT_EQ(base::UTF8ToUTF16(url.spec()), clipboard_result);
}

#if defined(OS_MACOSX)
#define MAYBE_WriteToClipboardMultipleURLs DISABLED_WriteToClipboardMultipleURLs
#else
#define MAYBE_WriteToClipboardMultipleURLs WriteToClipboardMultipleURLs
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_WriteToClipboardMultipleURLs) {
  BookmarkNodeData data;
  const BookmarkNode* root = model()->bookmark_bar_node();
  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("blah"));
  GURL url2(GURL("http://bar.com"));
  const base::string16 title2(ASCIIToUTF16("blah2"));
  const BookmarkNode* url_node = model()->AddURL(root, 0, title, url);
  const BookmarkNode* url_node2 = model()->AddURL(root, 1, title2, url2);
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(url_node);
  nodes.push_back(url_node2);

  data.ReadFromVector(nodes);
  data.WriteToClipboard();

  // Now read the data back in.
  base::string16 combined_text;
#if defined(OS_WIN)
  base::string16 new_line = base::ASCIIToUTF16("\r\n");
#else
  base::string16 new_line = base::ASCIIToUTF16("\n");
#endif
  combined_text = base::UTF8ToUTF16(url.spec()) + new_line
    + base::UTF8ToUTF16(url2.spec());
  base::string16 clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste, &clipboard_result);
  EXPECT_EQ(combined_text, clipboard_result);
}

#if defined(OS_MACOSX)
#define MAYBE_WriteToClipboardEmptyFolder DISABLED_WriteToClipboardEmptyFolder
#else
#define MAYBE_WriteToClipboardEmptyFolder WriteToClipboardEmptyFolder
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_WriteToClipboardEmptyFolder) {
  BookmarkNodeData data;
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, ASCIIToUTF16("g1"));
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(folder);

  data.ReadFromVector(nodes);
  data.WriteToClipboard();

  // Now read the data back in.
  base::string16 clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste, &clipboard_result);
  EXPECT_EQ(base::ASCIIToUTF16("g1"), clipboard_result);
}

TEST_F(BookmarkNodeDataTest, WriteToClipboardFolderWithChildren) {
  BookmarkNodeData data;
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, ASCIIToUTF16("g1"));
  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("blah"));
  model()->AddURL(folder, 0, title, url);
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(folder);

  data.ReadFromVector(nodes);
  data.WriteToClipboard();

  // Now read the data back in.
  base::string16 clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste, &clipboard_result);
  EXPECT_EQ(base::ASCIIToUTF16("g1"), clipboard_result);
}

// TODO(https://crbug.com/1010415): This test is flaky on various platforms, fix
// and re-enable it.
TEST_F(BookmarkNodeDataTest, DISABLED_WriteToClipboardFolderAndURL) {
  BookmarkNodeData data;
  GURL url(GURL("http://foo.com"));
  const base::string16 title(ASCIIToUTF16("blah"));
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* url_node = model()->AddURL(root, 0, title, url);
  const BookmarkNode* folder = model()->AddFolder(root, 0, ASCIIToUTF16("g1"));
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(url_node);
  nodes.push_back(folder);

  data.ReadFromVector(nodes);
  data.WriteToClipboard();

  // Now read the data back in.
  base::string16 combined_text;
#if defined(OS_WIN)
  base::string16 new_line = base::ASCIIToUTF16("\r\n");
#else
  base::string16 new_line = base::ASCIIToUTF16("\n");
#endif
  base::string16 folder_title = ASCIIToUTF16("g1");
  combined_text = base::ASCIIToUTF16(url.spec()) + new_line + folder_title;
  base::string16 clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste, &clipboard_result);
  EXPECT_EQ(combined_text, clipboard_result);
}

// Tests reading/writing of meta info.
TEST_F(BookmarkNodeDataTest, MetaInfo) {
  // Create a node containing meta info.
  const BookmarkNode* node = model()->AddURL(model()->other_node(),
                                             0,
                                             ASCIIToUTF16("foo bar"),
                                             GURL("http://www.google.com"));
  model()->SetNodeMetaInfo(node, "somekey", "somevalue");
  model()->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  BookmarkNodeData node_data(node);
  ui::OSExchangeData data;
  node_data.Write(GetProfilePath(), &data);

  // Read the data back in.
  ui::OSExchangeData data2(CloneProvider(data));
  BookmarkNodeData read_data;
  EXPECT_TRUE(read_data.Read(data2));
  EXPECT_TRUE(read_data.is_valid());
  ASSERT_EQ(1u, read_data.size());

  // Verify that the read data contains the same meta info.
  BookmarkNode::MetaInfoMap meta_info_map = read_data.elements[0].meta_info_map;
  EXPECT_EQ(2u, meta_info_map.size());
  EXPECT_EQ("somevalue", meta_info_map["somekey"]);
  EXPECT_EQ("someothervalue", meta_info_map["someotherkey"]);
}

#if !defined(OS_MACOSX)
TEST_F(BookmarkNodeDataTest, ReadFromPickleTooManyNodes) {
  // Test case determined by a fuzzer. See https://crbug.com/956583.
  const char pickled_data[] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0xff, 0x03, 0x03, 0x41};
  base::Pickle pickle(pickled_data, sizeof(pickled_data));
  BookmarkNodeData bookmark_node_data;
  EXPECT_FALSE(bookmark_node_data.ReadFromPickle(&pickle));
}
#endif

}  // namespace bookmarks
