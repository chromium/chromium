// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/bookmarks/bookmark_test_utils.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace {

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;

template <class T>
class BookmarkUIOperationsHelperTest : public testing::Test {
 public:
  BookmarkUIOperationsHelperTest() {
    ui::TestClipboard::CreateForCurrentThread();
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    profile_->GetTestingPrefService()->SetManagedPref(
        bookmarks::prefs::kManagedBookmarks,
        base::Value::List().Append(
            base::Value::Dict()
                .Set("name", "Google")
                .Set("url", GURL("http://google.com/").spec())));
    CHECK(managed_bookmark_service());

    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmark_merged_surface_service_ =
        BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_.get());
    WaitForBookmarkMergedSurfaceServiceToLoad(bookmark_merged_surface_service_);

    CHECK(managed_bookmark_service()->managed_node());
  }

  ~BookmarkUIOperationsHelperTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  TestingProfile* profile() { return profile_.get(); }

  bookmarks::BookmarkModel* model() { return model_; }

  bookmarks::ManagedBookmarkService* managed_bookmark_service() {
    return ManagedBookmarkServiceFactory::GetForProfile(profile());
  }

  internal::BookmarkUIOperationsHelper* CreateHelper(
      const BookmarkNode* parent) {
    if (std::is_same<T, BookmarkUIOperationsHelperNonMergedSurfaces>::value) {
      CHECK(parent->is_folder());
      helper_ = std::make_unique<BookmarkUIOperationsHelperNonMergedSurfaces>(
          model_.get(), parent);
    } else if (std::is_same<T,
                            BookmarkUIOperationsHelperMergedSurfaces>::value) {
      parent_folder_ = BookmarkParentFolder::FromFolderNode(parent);
      helper_ = std::make_unique<BookmarkUIOperationsHelperMergedSurfaces>(
          bookmark_merged_surface_service_.get(), &parent_folder_.value());
    }
    return helper_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_merged_surface_service_ =
      nullptr;
  std::optional<BookmarkParentFolder> parent_folder_;
  std::unique_ptr<internal::BookmarkUIOperationsHelper> helper_;
};

using testing::Types;

typedef Types<BookmarkUIOperationsHelperNonMergedSurfaces,
              BookmarkUIOperationsHelperMergedSurfaces>
    Implementations;

TYPED_TEST_SUITE(BookmarkUIOperationsHelperTest, Implementations);

TYPED_TEST(BookmarkUIOperationsHelperTest,
           DropBookmarksWithCopyFromSameProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = this->model()->bookmark_bar_node();
  this->model()->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      this->model()->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      this->model()->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  bookmark_node_data.SetOriginatingProfilePath(this->profile()->GetPath());

  internal::BookmarkUIOperationsHelper* helper = this->CreateHelper(bb_node);
  // Make a copy of `folder_child_node` added to the bookmark bar node.
  EXPECT_EQ(
      helper->DropBookmarks(
          this->profile(), bookmark_node_data, /*index=*/0,
          /*copy=*/true, chrome::BookmarkReorderDropTarget::kBookmarkBarView),
      ui::mojom::DragOperation::kCopy);
  ASSERT_EQ(3u, bb_node->children().size());
  const bookmarks::BookmarkNode* newly_copied_node =
      bb_node->children()[0].get();
  EXPECT_EQ(folder_child_node->GetTitle(), newly_copied_node->GetTitle());
  EXPECT_EQ(folder_child_node->url(), newly_copied_node->url());
}

TYPED_TEST(BookmarkUIOperationsHelperTest,
           DropBookmarksWithMoveFromSameProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = this->model()->bookmark_bar_node();
  this->model()->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      this->model()->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      this->model()->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  bookmark_node_data.SetOriginatingProfilePath(this->profile()->GetPath());

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(this->model()->other_node());
  // Make a move of `folder_child_node` from bookmark bar node to other node.
  EXPECT_EQ(
      helper->DropBookmarks(
          this->profile(), bookmark_node_data, /*index=*/0,
          /*copy=*/false, chrome::BookmarkReorderDropTarget::kBookmarkBarView),
      ui::mojom::DragOperation::kMove);
  EXPECT_EQ(2u, bb_node->children().size());
  EXPECT_EQ(this->model()->other_node()->children()[0].get(),
            folder_child_node);
}

TYPED_TEST(BookmarkUIOperationsHelperTest, DropBookmarksFromAnotherProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = this->model()->bookmark_bar_node();
  this->model()->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      this->model()->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      this->model()->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  // Profile path is empty for `bookmark_node_data`.
  EXPECT_FALSE(
      bookmark_node_data.IsFromProfilePath(this->profile()->GetPath()));

  internal::BookmarkUIOperationsHelper* helper = this->CreateHelper(folder);

  // Use `copy` false as it shouldn't matter, as long as the data is not from
  // the same profile, data should be copied.
  EXPECT_EQ(
      helper->DropBookmarks(
          this->profile(), bookmark_node_data, /*index=*/0,
          /*copy=*/false, chrome::BookmarkReorderDropTarget::kBookmarkBarView),
      ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(folder->children().size(), 2u);
  const bookmarks::BookmarkNode* newly_copied_node =
      folder->children()[0].get();
  EXPECT_EQ(newly_copied_node->url(), folder_child_node->url());
  EXPECT_EQ(folder->children()[1].get(), folder_child_node);
}

// `BookmarkNodeData` has a different implementation on Mac. It doesn't use
// `TestClipboard` if it is set. Given the clipboard is a shared resource,
// testing on Mac will be flaky. Refactoring is required to be able to test
// copy/paste to clipboard on Mac.
#if !BUILDFLAG(IS_MAC)
TYPED_TEST(BookmarkUIOperationsHelperTest, PasteBookmarkFromURL) {
  BookmarkModel* model = this->model();
  const std::u16string url_text = u"http://www.google.com/";
  const BookmarkNode* new_folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"New_Folder");
  // Write blank text to clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(std::u16string());
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(bookmarks::BookmarkNodeData::ClipboardContainsBookmarks());

  internal::BookmarkUIOperationsHelper* helper = this->CreateHelper(new_folder);
  EXPECT_FALSE(helper->CanPasteFromClipboard());

  // Write some valid url to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(url_text);
  }
  // Now we should be able to paste from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  helper->PasteFromClipboard(0);
  ASSERT_EQ(1u, new_folder->children().size());

  // Url for added node should be same as url_text.
  EXPECT_EQ(url_text,
            ASCIIToUTF16(new_folder->children().front()->url().spec()));
}

// Test for updating title such that url and title pair are unique among the
// children of parent.
TYPED_TEST(BookmarkUIOperationsHelperTest, MakeTitleUnique) {
  const std::u16string url_text = u"http://www.google.com/";
  const std::u16string title_text = u"foobar";
  BookmarkModel* model = this->model();
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();

  const BookmarkNode* node =
      model->AddURL(bookmark_bar_node, 0, title_text, GURL(url_text));

  EXPECT_EQ(url_text,
            ASCIIToUTF16(bookmark_bar_node->children()[0]->url().spec()));
  EXPECT_EQ(title_text, bookmark_bar_node->children()[0]->GetTitle());

  // Copy a node to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{node};
  internal::BookmarkUIOperationsHelper::CopyToClipboard(
      model, nodes, bookmarks::metrics::BookmarkEditSource::kOther,
      /*is_off_the_record=*/false);

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(bookmark_bar_node);
  // Now we should be able to paste from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  helper->PasteFromClipboard(1);
  ASSERT_EQ(2u, bookmark_bar_node->children().size());

  // Url for added node should be same as url_text.
  EXPECT_EQ(url_text,
            ASCIIToUTF16(bookmark_bar_node->children()[1]->url().spec()));
  // Title for added node should be numeric subscript suffix with copied node
  // title.
  EXPECT_EQ(u"foobar (1)", bookmark_bar_node->children()[1]->GetTitle());
}

TYPED_TEST(BookmarkUIOperationsHelperTest, CopyPasteMetaInfo) {
  BookmarkModel* model = this->model();
  const BookmarkNode* node = model->AddURL(model->other_node(), 0, u"foo bar",
                                           GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "somekey", "somevalue");
  model->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Copy a node to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{node};
  internal::BookmarkUIOperationsHelper::CopyToClipboard(
      model, nodes, bookmarks::metrics::BookmarkEditSource::kOther,
      /*is_off_the_record=*/false);

  // Paste node to a different folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder");
  EXPECT_EQ(0u, folder->children().size());

  internal::BookmarkUIOperationsHelper* helper = this->CreateHelper(folder);
  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  helper->PasteFromClipboard(0);
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

TYPED_TEST(BookmarkUIOperationsHelperTest, CopyPaste) {
  BookmarkModel* model = this->model();
  const BookmarkNode* node = model->AddURL(model->other_node(), 0, u"foo bar ",
                                           GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{node};
  internal::BookmarkUIOperationsHelper::CopyToClipboard(
      model, nodes, bookmarks::metrics::BookmarkEditSource::kOther,
      /*is_off_the_record=*/false);

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(model->bookmark_bar_node());

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(u"foo");
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(helper->CanPasteFromClipboard());
}

TYPED_TEST(BookmarkUIOperationsHelperTest, CopyPasteMultipleNodes) {
  BookmarkModel* model = this->model();
  AddNodesFromModelString(model, model->bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  size_t bookmark_bar_children = model->bookmark_bar_node()->children().size();
  const BookmarkNode* n1 = model->AddURL(model->other_node(), 0, u"foo bar 1 ",
                                         GURL("http://www.google.com"));
  const BookmarkNode* n2 = model->AddURL(model->other_node(), 1, u"foo bar 2 ",
                                         GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{n1, n2};
  internal::BookmarkUIOperationsHelper::CopyToClipboard(
      model, nodes, bookmarks::metrics::BookmarkEditSource::kOther,
      /*is_off_the_record=*/false);

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(model->bookmark_bar_node());

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  helper->PasteFromClipboard(1);
  EXPECT_EQ(model->bookmark_bar_node()->children().size(),
            bookmark_bar_children + 2u);
  CHECK_EQ(model->bookmark_bar_node()->children()[1]->GetTitle(),
           u"foo bar 1 ");
  CHECK_EQ(model->bookmark_bar_node()->children()[2]->GetTitle(),
           u"foo bar 2 ");
}

TYPED_TEST(BookmarkUIOperationsHelperTest, CutToClipboard) {
  BookmarkModel* model = this->model();
  bookmarks::MockBookmarkModelObserver observer;
  base::ScopedObservation<BookmarkModel, bookmarks::BookmarkModelObserver>
      model_observation{&observer};
  model_observation.Observe(model);

  GURL url("http://foo.com");
  const BookmarkNode* n1 =
      model->AddURL(model->other_node(), 0, u"foo bar 1 ", url);
  const BookmarkNode* n2 =
      model->AddURL(model->other_node(), 1, u"foo bar 2 ", url);

  EXPECT_CALL(observer, GroupedBookmarkChangesBeginning()).Times(2);
  EXPECT_CALL(observer, GroupedBookmarkChangesEnded()).Times(2);
  // Cut the nodes to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{n1, n2};
  internal::BookmarkUIOperationsHelper::CutToClipboard(
      model, nodes, bookmarks::metrics::BookmarkEditSource::kOther,
      /*is_off_the_record=*/false);

  // Make sure the nodes were removed.
  EXPECT_EQ(0u, model->other_node()->children().size());

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(model->other_node());
  // And make sure we can paste from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  helper->PasteFromClipboard(0);
  EXPECT_EQ(model->other_node()->children().size(), 2u);
  CHECK_EQ(model->other_node()->children()[0]->GetTitle(), u"foo bar 1 ");
  CHECK_EQ(model->other_node()->children()[1]->GetTitle(), u"foo bar 2 ");
}

TYPED_TEST(BookmarkUIOperationsHelperTest, PasteNonEditableNodes) {
  // Load a model with an managed node that is not editable.
  BookmarkModel* model = this->model();
  const BookmarkNode* node = model->AddURL(model->other_node(), 0, u"foo bar ",
                                           GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{node};
  internal::BookmarkUIOperationsHelper::CopyToClipboard(
      model, nodes, bookmarks::metrics::BookmarkEditSource::kOther,
      /*is_off_the_record=*/false);

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(model->bookmark_bar_node());
  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(helper->CanPasteFromClipboard());

  // But it can't be pasted into a non-editable folder.
  helper = this->CreateHelper(this->managed_bookmark_service()->managed_node());
  EXPECT_FALSE(helper->CanPasteFromClipboard());
}

#endif  // !BUILDFLAG(IS_MAC)

TEST(BookmarkUIOperationsHelperMergedSurfacesTest,
     GetDefaultParentForNonMergedSurfacesWithAccountPermanentNodes) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  bookmarks::BookmarkModel model(
      std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(&model,
                                       /*managed_bookmark_service=*/nullptr);
  service.LoadForTesting({});
  model.LoadEmptyForTest();
  model.CreateAccountPermanentFolders();

  ASSERT_TRUE(model.account_bookmark_bar_node());
  BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
  EXPECT_EQ(BookmarkUIOperationsHelperMergedSurfaces(&service, &folder)
                .GetDefaultParentForNonMergedSurfaces(),
            model.account_bookmark_bar_node());
}

TEST(BookmarkUIOperationsHelperMergedSurfacesTest,
     GetDefaultParentForNonMergedSurfacesWithoutAccountPermanentNodes) {
  bookmarks::BookmarkModel model(
      std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(&model,
                                       /*managed_bookmark_service=*/nullptr);
  service.LoadForTesting({});
  model.LoadEmptyForTest();
  ASSERT_FALSE(model.account_bookmark_bar_node());
  BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
  EXPECT_EQ(BookmarkUIOperationsHelperMergedSurfaces(&service, &folder)
                .GetDefaultParentForNonMergedSurfaces(),
            model.bookmark_bar_node());
}

TEST(BookmarkUIOperationsHelperMergedSurfacesTest,
     GetDefaultParentForNonMergedSurfacesNonPermanentFolder) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  bookmarks::BookmarkModel model(
      std::make_unique<bookmarks::TestBookmarkClient>());
  BookmarkMergedSurfaceService service(&model,
                                       /*managed_bookmark_service=*/nullptr);
  model.LoadEmptyForTest();
  service.LoadForTesting({});
  {
    // Test regular non permanent node.
    const BookmarkNode* node =
        model.AddFolder(model.bookmark_bar_node(), 0, u"folder");
    BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
    EXPECT_EQ(BookmarkUIOperationsHelperMergedSurfaces(&service, &folder)
                  .GetDefaultParentForNonMergedSurfaces(),
              node);
  }

  {
    model.CreateAccountPermanentFolders();
    ASSERT_TRUE(model.account_bookmark_bar_node());
    const BookmarkNode* node =
        model.AddFolder(model.account_bookmark_bar_node(), 0, u"folder");
    BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(node);
    EXPECT_EQ(BookmarkUIOperationsHelperMergedSurfaces(&service, &folder)
                  .GetDefaultParentForNonMergedSurfaces(),
              node);
  }
}

TEST(BookmarkUIOperationsHelperMergedSurfacesTest,
     GetDefaultParentForNonMergedSurfacesForManagedNodes) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  std::unique_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service(
      CreateManagedBookmarkService(&prefs, 10));
  BookmarkModel model(std::make_unique<TestBookmarkClientWithManagedService>(
      managed_bookmark_service.get()));
  BookmarkMergedSurfaceService service(&model, managed_bookmark_service.get());
  model.LoadEmptyForTest();
  service.LoadForTesting({});

  const BookmarkNode* managed_node = managed_bookmark_service->managed_node();
  ASSERT_TRUE(managed_node);

  BookmarkParentFolder managed_peranent_folder =
      BookmarkParentFolder::ManagedFolder();
  EXPECT_EQ(BookmarkUIOperationsHelperMergedSurfaces(&service,
                                                     &managed_peranent_folder)
                .GetDefaultParentForNonMergedSurfaces(),
            managed_node);

  const BookmarkNode* managed_new_folder =
      model.AddFolder(managed_node, 0, u"New Folder");
  BookmarkParentFolder folder =
      BookmarkParentFolder::FromFolderNode(managed_new_folder);
  EXPECT_EQ(BookmarkUIOperationsHelperMergedSurfaces(&service, &folder)
                .GetDefaultParentForNonMergedSurfaces(),
            managed_new_folder);
}

}  // namespace
