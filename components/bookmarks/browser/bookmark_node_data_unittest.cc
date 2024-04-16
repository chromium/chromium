// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace bookmarks {

class BookmarkNodeDataTest : public testing::Test {
 public:
  BookmarkNodeDataTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  BookmarkNodeDataTest(const BookmarkNodeDataTest&) = delete;
  BookmarkNodeDataTest& operator=(const BookmarkNodeDataTest&) = delete;

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
};

namespace {

std::unique_ptr<ui::OSExchangeDataProvider> CloneProvider(
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
// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC)
#define MAYBE_JustURL DISABLED_JustURL
#else
#define MAYBE_JustURL JustURL
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_JustURL) {
  const GURL url("http://google.com");
  const std::u16string title(u"google.com");

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

// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC)
#define MAYBE_URL DISABLED_URL
#else
#define MAYBE_URL URL
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_URL) {
  // Write a single node representing a URL to the clipboard.
  const BookmarkNode* root = model()->bookmark_bar_node();
  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"foo.com");
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
  std::optional<ui::OSExchangeData::UrlInfo> url_info =
      data2.GetURLAndTitle(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  ASSERT_TRUE(url_info.has_value());
  EXPECT_EQ(url, url_info->url);
  EXPECT_EQ(title, url_info->title);
}

// Tests writing a folder to the clipboard.
// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC)
#define MAYBE_Folder DISABLED_Folder
#else
#define MAYBE_Folder Folder
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_Folder) {
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* g1 = model()->AddFolder(root, 0, u"g1");
  model()->AddFolder(g1, 0, u"g11");
  const BookmarkNode* g12 = model()->AddFolder(g1, 0, u"g12");

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
// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC)
#define MAYBE_FolderWithChild DISABLED_FolderWithChild
#else
#define MAYBE_FolderWithChild FolderWithChild
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_FolderWithChild) {
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, u"g1");

  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"blah2");

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
// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultipleNodes DISABLED_MultipleNodes
#else
#define MAYBE_MultipleNodes MultipleNodes
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_MultipleNodes) {
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, u"g1");

  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"blah2");

  const BookmarkNode* url_node = model()->AddURL(folder, 0, title, url);

  // Write the nodes to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
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
  EXPECT_EQ(u"g1", read_folder.title);
  EXPECT_EQ(1u, read_folder.children.size());

  const BookmarkNodeData::Element& read_url = read_data.elements[1];
  EXPECT_TRUE(read_url.is_url);
  EXPECT_EQ(title, read_url.title);
  EXPECT_EQ(0u, read_url.children.size());

  // And make sure we get the node back.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> read_nodes =
      read_data.GetNodes(model(), GetProfilePath());
  ASSERT_EQ(2u, read_nodes.size());
  EXPECT_TRUE(read_nodes[0] == folder);
  EXPECT_TRUE(read_nodes[1] == url_node);

  // Asking for the first node should return NULL with more than one element
  // present.
  EXPECT_TRUE(read_data.GetFirstNode(model(), GetProfilePath()) == nullptr);
}

TEST_F(BookmarkNodeDataTest, WriteToClipboardURL) {
  BookmarkNodeData data;
  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"blah");

  data.ReadFromTuple(url, title);
  data.WriteToClipboard(/*is_off_the_record=*/false);

  // Now read the data back in.
  std::u16string clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste,
                       /* data_dst = */ nullptr, &clipboard_result);
  EXPECT_EQ(base::UTF8ToUTF16(url.spec()), clipboard_result);
}

#if BUILDFLAG(IS_APPLE)
#define MAYBE_WriteToClipboardMultipleURLs DISABLED_WriteToClipboardMultipleURLs
#else
#define MAYBE_WriteToClipboardMultipleURLs WriteToClipboardMultipleURLs
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_WriteToClipboardMultipleURLs) {
  BookmarkNodeData data;
  const BookmarkNode* root = model()->bookmark_bar_node();
  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"blah");
  GURL url2(GURL("http://bar.com"));
  const std::u16string title2(u"blah2");
  const BookmarkNode* url_node = model()->AddURL(root, 0, title, url);
  const BookmarkNode* url_node2 = model()->AddURL(root, 1, title2, url2);
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  nodes.push_back(url_node);
  nodes.push_back(url_node2);

  data.ReadFromVector(nodes);
  data.WriteToClipboard(/*is_off_the_record=*/false);

  // Now read the data back in.
  std::u16string combined_text;
#if BUILDFLAG(IS_WIN)
  std::u16string new_line = u"\r\n";
#else
  std::u16string new_line = u"\n";
#endif
  combined_text = base::UTF8ToUTF16(url.spec()) + new_line
    + base::UTF8ToUTF16(url2.spec());
  std::u16string clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste,
                       /* data_dst = */ nullptr, &clipboard_result);
  EXPECT_EQ(combined_text, clipboard_result);
}

// Test is flaky on LaCrOS: crbug.com/1010185
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_WriteToClipboardEmptyFolder DISABLED_WriteToClipboardEmptyFolder
#else
#define MAYBE_WriteToClipboardEmptyFolder WriteToClipboardEmptyFolder
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_WriteToClipboardEmptyFolder) {
  BookmarkNodeData data;
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, u"g1");
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  nodes.push_back(folder);

  data.ReadFromVector(nodes);
  data.WriteToClipboard(/*is_off_the_record=*/false);

  // Now read the data back in.
  std::u16string clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste,
                       /* data_dst = */ nullptr, &clipboard_result);
  EXPECT_EQ(u"g1", clipboard_result);
}

// Test is flaky on LaCrOS: crbug.com/1010353
// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_WriteToClipboardFolderWithChildren \
  DISABLED_WriteToClipboardFolderWithChildren
#else
#define MAYBE_WriteToClipboardFolderWithChildren \
  WriteToClipboardFolderWithChildren
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_WriteToClipboardFolderWithChildren) {
  BookmarkNodeData data;
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* folder = model()->AddFolder(root, 0, u"g1");
  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"blah");
  model()->AddURL(folder, 0, title, url);
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  nodes.push_back(folder);

  data.ReadFromVector(nodes);
  data.WriteToClipboard(/*is_off_the_record=*/false);

  // Now read the data back in.
  std::u16string clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste,
                       /* data_dst = */ nullptr, &clipboard_result);
  EXPECT_EQ(u"g1", clipboard_result);
}

// TODO(crbug.com/40651106): This test is failing on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WriteToClipboardFolderAndURL DISABLED_WriteToClipboardFolderAndURL
#else
#define MAYBE_WriteToClipboardFolderAndURL WriteToClipboardFolderAndURL
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_WriteToClipboardFolderAndURL) {
  BookmarkNodeData data;
  GURL url(GURL("http://foo.com"));
  const std::u16string title(u"blah");
  const BookmarkNode* root = model()->bookmark_bar_node();
  const BookmarkNode* url_node = model()->AddURL(root, 0, title, url);
  const BookmarkNode* folder = model()->AddFolder(root, 0, u"g1");
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  nodes.push_back(url_node);
  nodes.push_back(folder);

  data.ReadFromVector(nodes);
  data.WriteToClipboard(/*is_off_the_record=*/false);

  // Now read the data back in.
  std::u16string combined_text;
#if BUILDFLAG(IS_WIN)
  std::u16string new_line = u"\r\n";
#else
  std::u16string new_line = u"\n";
#endif
  std::u16string folder_title = u"g1";
  combined_text = base::ASCIIToUTF16(url.spec()) + new_line + folder_title;
  std::u16string clipboard_result;
  clipboard().ReadText(ui::ClipboardBuffer::kCopyPaste,
                       /* data_dst = */ nullptr, &clipboard_result);
  EXPECT_EQ(combined_text, clipboard_result);
}

// Tests reading/writing of meta info.
// Test is flaky on Mac: crbug.com/1236362
#if BUILDFLAG(IS_MAC)
#define MAYBE_MetaInfo DISABLED_MetaInfo
#else
#define MAYBE_MetaInfo MetaInfo
#endif
TEST_F(BookmarkNodeDataTest, MAYBE_MetaInfo) {
  // Create a node containing meta info.
  const BookmarkNode* node = model()->AddURL(
      model()->other_node(), 0, u"foo bar", GURL("http://www.google.com"));
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

#if !BUILDFLAG(IS_APPLE)
TEST_F(BookmarkNodeDataTest, ReadFromPickleTooManyNodes) {
  // Test case determined by a fuzzer. See https://crbug.com/956583.
  const uint8_t pickled_data[] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0xff, 0x03, 0x03, 0x41};
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(pickled_data);
  BookmarkNodeData bookmark_node_data;
  EXPECT_FALSE(bookmark_node_data.ReadFromPickle(&pickle));
}
#endif

}  // namespace bookmarks
