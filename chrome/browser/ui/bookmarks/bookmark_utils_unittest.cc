// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace {

class BookmarkUtilsGetBookmarkDropOperationTest : public testing::Test {
 public:
  BookmarkUtilsGetBookmarkDropOperationTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    profile_->GetTestingPrefService()->SetManagedPref(
        bookmarks::prefs::kManagedBookmarks,
        base::Value(base::Value::List().Append(
            base::Value::Dict()
                .Set("name", "managed_bookmark")
                .Set("url", GURL("http://google.com/").spec()))));
    model()->LoadEmptyForTest();
  }

  TestingProfile* profile() { return profile_.get(); }

  bookmarks::BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  const bookmarks::BookmarkNode* managed_node() {
    return ManagedBookmarkServiceFactory::GetForProfile(profile())
        ->managed_node();
  }

  void AddNodesToBookmarkBarFromModelString(const std::string& string) {
    bookmarks::test::AddNodesFromModelString(
        model(), model()->bookmark_bar_node(), string);
  }

 private:
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest, DropURL) {
  ui::OSExchangeData os_drag_data;
  os_drag_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_LINK);
  bookmarks::BookmarkNodeData drag_node_data;
  drag_node_data.Read(os_drag_data);

  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_node_data,
                BookmarkParentFolder::BookmarkBarFolder(), 0),
            ui::mojom::DragOperation::kLink);
}

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest, DropNodeFromSameProfile) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  ui::OSExchangeData os_drag_data;
  {
    bookmarks::BookmarkNodeData drag_data(
        model()->bookmark_bar_node()->children()[1].get());
    drag_data.Write(profile()->GetPath(), &os_drag_data);
  }
  ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_MOVE);
  bookmarks::BookmarkNodeData drag_data;
  drag_data.Read(os_drag_data);

  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_data,
                BookmarkParentFolder::BookmarkBarFolder(), 0),
            ui::mojom::DragOperation::kMove);
}

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest,
       DropNodeFromDifferentProfile) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  ui::OSExchangeData os_drag_data;
  {
    bookmarks::BookmarkNodeData drag_data(
        model()->bookmark_bar_node()->children()[1].get());
    drag_data.Write(base::FilePath(FILE_PATH_LITERAL("/tmp/differentProfile")),
                    &os_drag_data);
  }
  ui::DropTargetEvent target_event(
      os_drag_data, gfx::PointF(), gfx::PointF(),
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  bookmarks::BookmarkNodeData drag_data;
  drag_data.Read(os_drag_data);

  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_data,
                BookmarkParentFolder::BookmarkBarFolder(), 1),
            ui::mojom::DragOperation::kCopy);
}

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest, DropMultipleNodes) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  ui::OSExchangeData os_drag_data;
  {
    bookmarks::BookmarkNodeData drag_data(
        {model()->bookmark_bar_node()->children()[1].get(),
         model()->bookmark_bar_node()->children()[2].get()});
    drag_data.Write(profile()->GetPath(), &os_drag_data);
  }
  ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_MOVE);

  bookmarks::BookmarkNodeData drag_data;
  drag_data.Read(os_drag_data);

  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_data,
                BookmarkParentFolder::BookmarkBarFolder(), 0),
            ui::mojom::DragOperation::kNone);
}

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest, DropNodeInSamePosition) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  ui::OSExchangeData os_drag_data;
  {
    bookmarks::BookmarkNodeData drag_data(
        model()->bookmark_bar_node()->children()[1].get());
    drag_data.Write(profile()->GetPath(), &os_drag_data);
  }
  ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_MOVE);

  bookmarks::BookmarkNodeData drag_data;
  drag_data.Read(os_drag_data);

  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_data,
                BookmarkParentFolder::BookmarkBarFolder(), 1),
            ui::mojom::DragOperation::kNone);
  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_data,
                BookmarkParentFolder::BookmarkBarFolder(), 2),
            ui::mojom::DragOperation::kNone);
}

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest, DropOnManagedNode) {
  {
    // Drop URL.
    ui::OSExchangeData os_drag_data;
    os_drag_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
    ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                     ui::DragDropTypes::DRAG_LINK);
    bookmarks::BookmarkNodeData drag_node_data;
    drag_node_data.Read(os_drag_data);

    EXPECT_EQ(chrome::GetBookmarkDropOperation(
                  profile(), target_event, drag_node_data,
                  BookmarkParentFolder::ManagedFolder(), 0),
              ui::mojom::DragOperation::kNone);
  }

  {
    // Drop bookmark node.
    AddNodesToBookmarkBarFromModelString("a b c d e f ");
    ui::OSExchangeData os_drag_data;
    {
      bookmarks::BookmarkNodeData drag_data(
          model()->bookmark_bar_node()->children()[1].get());
      drag_data.Write(profile()->GetPath(), &os_drag_data);
    }
    ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                     ui::DragDropTypes::DRAG_MOVE);
    bookmarks::BookmarkNodeData drag_data;
    drag_data.Read(os_drag_data);

    EXPECT_EQ(chrome::GetBookmarkDropOperation(
                  profile(), target_event, drag_data,
                  BookmarkParentFolder::ManagedFolder(), 0),
              ui::mojom::DragOperation::kNone);
  }
}

TEST_F(BookmarkUtilsGetBookmarkDropOperationTest, DropManagedNode) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  ui::OSExchangeData os_drag_data;
  {
    bookmarks::BookmarkNodeData drag_data(managed_node()->children()[0].get());
    drag_data.Write(profile()->GetPath(), &os_drag_data);
  }
  ui::DropTargetEvent target_event(os_drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_NONE);
  bookmarks::BookmarkNodeData drag_data;
  drag_data.Read(os_drag_data);

  EXPECT_EQ(chrome::GetBookmarkDropOperation(
                profile(), target_event, drag_data,
                BookmarkParentFolder::BookmarkBarFolder(), 1),
            ui::mojom::DragOperation::kCopy);
}

}  // namespace
