// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmarks_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/browser_apis/bookmarks/testing/default_bookmarks_view.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace bookmarks_api {

namespace {

class BookmarksServiceImplTest : public testing::Test {
 public:
  base::Uuid GetUuid(const bookmarks::BookmarkNode* node) {
    return view_->GetUuid(node);
  }

 protected:
  BookmarksServiceImplTest() {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client_ = client.get();
    model_ = std::make_unique<bookmarks::BookmarkModel>(std::move(client));
    model_->LoadEmptyForTest();
    auto view = std::make_unique<DefaultBookmarksView>(model_.get());
    view_ = view.get();
    service_ = std::make_unique<BookmarksServiceImpl>(std::move(view));

    mojo::PendingRemote<mojom::BookmarksService> pending_remote;
    service_->Accept(pending_remote.InitWithNewPipeAndPassReceiver());
    remote_service_.Bind(std::move(pending_remote));
  }

  void TearDown() override {
    client_ = nullptr;
    view_ = nullptr;
    service_.reset();
    model_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<bookmarks::TestBookmarkClient> client_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<DefaultBookmarksView> view_;
  std::unique_ptr<BookmarksServiceImpl> service_;
  mojo::Remote<mojom::BookmarksService> remote_service_;
};

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/527150892): Permanent folders are handled differently on
// Android.
#define MAYBE_GetBookmarks_Empty DISABLED_GetBookmarks_Empty
#else
#define MAYBE_GetBookmarks_Empty GetBookmarks_Empty
#endif
TEST_F(BookmarksServiceImplTest, MAYBE_GetBookmarks_Empty) {
  base::RunLoop run_loop;
  remote_service_->GetBookmarks(base::BindOnce(
      [](base::OnceClosure quit_closure, BookmarksServiceImplTest* test,
         const bookmarks::BookmarkModel* model,
         mojom::BookmarksService::GetBookmarksResult result) {
        ASSERT_TRUE(result.has_value());
        const auto& root = result.value()->root;
        EXPECT_EQ(root->id, test->GetUuid(model->root_node()));

        ASSERT_EQ(root->children.size(), 3u);
        EXPECT_TRUE(root->children[0]->children.empty());
        // TODO(ffred): Add unit tests for `is_synced` field. We currently don't
        // test it because we don't have a way to mock the managed bookmarks
        // service.
        EXPECT_EQ(root->children[0]->permanent_folder_type,
                  mojom::PermanentFolderType::kBookmarkBar);
        EXPECT_TRUE(root->children[1]->children.empty());
        EXPECT_EQ(root->children[1]->permanent_folder_type,
                  mojom::PermanentFolderType::kOther);
        EXPECT_TRUE(root->children[2]->children.empty());
        EXPECT_EQ(root->children[2]->permanent_folder_type,
                  mojom::PermanentFolderType::kMobile);

        EXPECT_TRUE(result.value()->stream.is_valid());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), this, model_.get()));
  run_loop.Run();
}

TEST_F(BookmarksServiceImplTest, GetBookmark_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  model_->GetFavicon(node);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);

  ASSERT_TRUE(client_->SimulateFaviconLoaded(
      node->url(), GURL("http://example.com/favicon.ico"), image));

  auto result = service_->GetBookmark(GetUuid(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  const auto& url_node = result.value()->get_url();
  EXPECT_EQ(url_node->id, GetUuid(node));
  EXPECT_EQ(url_node->title, "Title");
  EXPECT_EQ(url_node->url, GURL("http://example.com"));
  ASSERT_TRUE(url_node->favicon_url.has_value());
  EXPECT_EQ(url_node->favicon_url.value(),
            GURL("http://example.com/favicon.ico"));
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_Bookmark_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result =
      service_->CreateBookmarkNode(GetUuid(parent), 0, std::move(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  const auto& created_url_node = result.value()->get_url();
  EXPECT_EQ(created_url_node->title, "New Bookmark");
  EXPECT_EQ(created_url_node->url, GURL("http://new-example.com"));

  // Verify it was actually added to the model.
  ASSERT_EQ(parent->children().size(), 1u);
  const bookmarks::BookmarkNode* model_node = parent->children()[0].get();
  ASSERT_TRUE(model_node);
  EXPECT_EQ(GetUuid(model_node), created_url_node->id.value());
  EXPECT_EQ(model_node->GetTitle(), u"New Bookmark");
  EXPECT_EQ(model_node->url(), GURL("http://new-example.com"));
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_Folder_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto folder_node = mojom::Folder::New();
  folder_node->title = "New Folder";
  auto node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result =
      service_->CreateBookmarkNode(GetUuid(parent), 0, std::move(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_folder());
  const auto& created_folder_node = result.value()->get_folder();
  EXPECT_EQ(created_folder_node->title, "New Folder");
  EXPECT_TRUE(created_folder_node->children.empty());
  EXPECT_FALSE(created_folder_node->permanent_folder_type.has_value());

  // Verify it was actually added to the model.
  ASSERT_EQ(parent->children().size(), 1u);
  const bookmarks::BookmarkNode* model_node = parent->children()[0].get();
  ASSERT_TRUE(model_node);
  EXPECT_EQ(GetUuid(model_node), created_folder_node->id.value());
  EXPECT_EQ(model_node->GetTitle(), u"New Folder");
  EXPECT_TRUE(model_node->is_folder());
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_Append_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  model_->AddURL(parent, 0, u"Existing", GURL("http://existing.com"));

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->CreateBookmarkNode(GetUuid(parent), std::nullopt,
                                             std::move(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  const auto& created_url_node = result.value()->get_url();

  ASSERT_EQ(parent->children().size(), 2u);
  EXPECT_EQ(parent->children()[1]->GetTitle(), u"New Bookmark");
  EXPECT_EQ(GetUuid(parent->children()[1].get()), created_url_node->id);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_NegativeIndex_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result =
      service_->CreateBookmarkNode(GetUuid(parent), -1, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_IndexOutOfRange_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  // Parent has 0 children, so index 1 is out of range.
  auto result =
      service_->CreateBookmarkNode(GetUuid(parent), 1, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_ParentNotFolder) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* url_node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto new_url_node = mojom::Url::New();
  new_url_node->title = "New Bookmark";
  new_url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(new_url_node));

  auto result =
      service_->CreateBookmarkNode(GetUuid(url_node), 0, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_NullNode_Error) {
  auto result = service_->CreateBookmarkNode(
      GetUuid(model_->bookmark_bar_node()), 0, nullptr);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_InvalidUrl) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("invalid-url");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result =
      service_->CreateBookmarkNode(GetUuid(parent), 0, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Title_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = GetUuid(node);
  url_node->title = "Updated Title";
  url_node->url = GURL("http://example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  EXPECT_EQ(result.value()->get_url()->title, "Updated Title");
  EXPECT_EQ(result.value()->get_url()->url, GURL("http://example.com"));

  EXPECT_EQ(node->GetTitle(), u"Updated Title");
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Url_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = GetUuid(node);
  url_node->title = "Title";
  url_node->url = GURL("http://updated-example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  EXPECT_EQ(result.value()->get_url()->title, "Title");
  EXPECT_EQ(result.value()->get_url()->url, GURL("http://updated-example.com"));

  EXPECT_EQ(node->url(), GURL("http://updated-example.com"));
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_AllFields_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = GetUuid(node);
  url_node->title = "Updated Title";
  url_node->url = GURL("http://updated-example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  EXPECT_EQ(result.value()->get_url()->title, "Updated Title");
  EXPECT_EQ(result.value()->get_url()->url, GURL("http://updated-example.com"));

  EXPECT_EQ(node->GetTitle(), u"Updated Title");
  EXPECT_EQ(node->url(), GURL("http://updated-example.com"));
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Folder_Title_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node = model_->AddFolder(parent, 0, u"Folder");

  auto folder_node = mojom::Folder::New();
  folder_node->id = GetUuid(node);
  folder_node->title = "Updated Folder";
  auto update_node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_folder());
  EXPECT_EQ(result.value()->get_folder()->title, "Updated Folder");

  EXPECT_EQ(node->GetTitle(), u"Updated Folder");
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Folder_Url_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node = model_->AddFolder(parent, 0, u"Folder");

  // We try to update a folder node with URL data.
  auto url_node = mojom::Url::New();
  url_node->id = GetUuid(node);
  url_node->title = "Updated Folder";
  url_node->url = GURL("http://example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_UrlNode_WithFolder_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  // We try to update a URL node with Folder data.
  auto folder_node = mojom::Folder::New();
  folder_node->id = GetUuid(node);
  folder_node->title = "New Folder Title";
  auto update_node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_InvalidUrl_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = GetUuid(node);
  url_node->title = "Title";
  url_node->url = GURL("invalid-url");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_PermanentNode_Error) {
  const bookmarks::BookmarkNode* node = model_->bookmark_bar_node();
  ASSERT_TRUE(node);

  auto folder_node = mojom::Folder::New();
  folder_node->id = GetUuid(node);
  folder_node->title = "Updated Title";
  auto update_node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, MoveBookmarkNode_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(parent, 0, u"Folder");
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 1, u"Title", GURL("http://example.com"));

  ASSERT_EQ(parent->children().size(), 2u);
  ASSERT_EQ(folder->children().size(), 0u);

  auto result = service_->MoveBookmarkNode(GetUuid(node), GetUuid(folder), 0);
  ASSERT_TRUE(result.has_value());

  // Verify it was moved in the model.
  ASSERT_EQ(parent->children().size(), 1u);
  ASSERT_EQ(folder->children().size(), 1u);
  EXPECT_EQ(folder->children()[0].get(), node);
}

TEST_F(BookmarksServiceImplTest, MoveBookmarkNode_ParentNotFolder) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(parent, 1, u"Title2", GURL("http://example2.com"));

  auto result = service_->MoveBookmarkNode(GetUuid(node1), GetUuid(node2), 0);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, MoveBookmarkNode_PermanentNode_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(parent, 0, u"Folder");
  const bookmarks::BookmarkNode* node = model_->other_node();

  auto result = service_->MoveBookmarkNode(GetUuid(node), GetUuid(folder), 0);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, MoveBookmarkNode_IndexOutOfRange) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(parent, 0, u"Folder");
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 1, u"Title", GURL("http://example.com"));

  auto result = service_->MoveBookmarkNode(GetUuid(node), GetUuid(folder), 5);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, DeleteBookmarkNodes_PermanentNode_Error) {
  const bookmarks::BookmarkNode* node = model_->bookmark_bar_node();
  ASSERT_TRUE(node);

  auto result = service_->DeleteBookmarkNodes({GetUuid(node)});
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, DeleteBookmarkNodes_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(parent, 1, u"Title2", GURL("http://example2.com"));

  base::Uuid model_uuid1 = node1->uuid();
  base::Uuid model_uuid2 = node2->uuid();
  base::Uuid uuid1 = GetUuid(node1);
  base::Uuid uuid2 = GetUuid(node2);

  auto result = service_->DeleteBookmarkNodes({uuid1, uuid2});
  EXPECT_TRUE(result.has_value());

  // Verify they were removed from the model.
  EXPECT_FALSE(model_->GetNodeByUuid(
      model_uuid1,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_FALSE(model_->GetNodeByUuid(
      model_uuid2,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_TRUE(parent->children().empty());
}

TEST_F(BookmarksServiceImplTest, DeleteBookmarkNodes_OnePermanent_NoneDeleted) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  base::Uuid uuid = GetUuid(node);
  base::Uuid permanent_uuid = GetUuid(model_->bookmark_bar_node());

  auto result = service_->DeleteBookmarkNodes({uuid, permanent_uuid});
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);

  // Verify the non-permanent node was NOT removed.
  EXPECT_TRUE(model_->GetNodeByUuid(
      node->uuid(),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(parent->children().size(), 1u);
}

class TestBookmarksObserver : public mojom::BookmarksObserver {
 public:
  explicit TestBookmarksObserver(
      mojo::PendingAssociatedReceiver<mojom::BookmarksObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnBookmarksEvents(
      std::vector<mojom::BookmarksEventPtr> events) override {
    call_count_++;
    for (auto& event : events) {
      events_.push_back(std::move(event));
    }
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitForEvent() {
    if (!events_.empty()) {
      return;
    }
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  const std::vector<mojom::BookmarksEventPtr>& events() const {
    return events_;
  }
  int call_count() const { return call_count_; }
  void ClearEvents() {
    events_.clear();
    call_count_ = 0;
  }

 private:
  mojo::AssociatedReceiver<mojom::BookmarksObserver> receiver_;
  std::vector<mojom::BookmarksEventPtr> events_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  int call_count_ = 0;
};

TEST_F(BookmarksServiceImplTest, Observation) {
  mojom::BookmarksSnapshotPtr snapshot;
  {
    base::RunLoop run_loop;
    remote_service_->GetBookmarks(base::BindOnce(
        [](mojom::BookmarksSnapshotPtr* out_snapshot,
           base::OnceClosure quit_closure,
           mojom::BookmarksService::GetBookmarksResult result) {
          ASSERT_TRUE(result.has_value());
          *out_snapshot = std::move(result.value());
          std::move(quit_closure).Run();
        },
        &snapshot, run_loop.QuitClosure()));
    run_loop.Run();
  }

  ASSERT_TRUE(snapshot);
  TestBookmarksObserver observer(std::move(snapshot->stream));

  // Add a bookmark node.
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  observer.WaitForEvent();

  ASSERT_EQ(observer.events().size(), 1u);
  EXPECT_TRUE(observer.events()[0]->is_added());
  const auto& added = observer.events()[0]->get_added();
  EXPECT_EQ(added->parent_id, GetUuid(parent));
  EXPECT_EQ(added->index, 0);
  EXPECT_TRUE(added->node->is_url());
  EXPECT_EQ(added->node->get_url()->title, "Title");
  EXPECT_EQ(added->node->get_url()->url, GURL("http://example.com"));

  observer.ClearEvents();

  // Move the node.
  const bookmarks::BookmarkNode* other_parent = model_->other_node();
  ASSERT_TRUE(other_parent);
  const bookmarks::BookmarkNode* node = parent->children()[0].get();
  base::Uuid node_uuid = GetUuid(node);

  model_->Move(node, other_parent, 0);
  observer.WaitForEvent();

  ASSERT_EQ(observer.events().size(), 1u);
  EXPECT_TRUE(observer.events()[0]->is_moved());
  const auto& moved = observer.events()[0]->get_moved();
  EXPECT_EQ(moved->old_parent_id, GetUuid(parent));
  EXPECT_EQ(moved->old_index, 0);
  EXPECT_EQ(moved->new_parent_id, GetUuid(other_parent));
  EXPECT_EQ(moved->new_index, 0);

  observer.ClearEvents();

  // Change the node.
  model_->SetTitle(node, u"New Title",
                   bookmarks::metrics::BookmarkEditSource::kUser);
  observer.WaitForEvent();

  ASSERT_EQ(observer.events().size(), 1u);
  EXPECT_TRUE(observer.events()[0]->is_changed());
  const auto& changed = observer.events()[0]->get_changed();
  EXPECT_TRUE(changed->node->is_url());
  EXPECT_EQ(changed->node->get_url()->id, node_uuid);
  EXPECT_EQ(changed->node->get_url()->title, "New Title");

  observer.ClearEvents();

  // Add another node to other_parent.
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(other_parent, 1, u"Title 2", GURL("http://example2.com"));
  observer.WaitForEvent();
  observer.ClearEvents();

  // Reorder other_parent children: [node, node2] -> [node2, node]
  std::vector<const bookmarks::BookmarkNode*> new_order = {node2, node};
  model_->ReorderChildren(other_parent, new_order);
  observer.WaitForEvent();

  ASSERT_EQ(observer.events().size(), 1u);
  EXPECT_TRUE(observer.events()[0]->is_moved());
  const auto& moved2 = observer.events()[0]->get_moved();
  EXPECT_EQ(moved2->old_parent_id, GetUuid(other_parent));
  EXPECT_EQ(moved2->old_index, 1);  // node2 was at index 1
  EXPECT_EQ(moved2->new_parent_id, GetUuid(other_parent));
  EXPECT_EQ(moved2->new_index, 0);  // moved to index 0

  observer.ClearEvents();

  // Remove the node.
  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);
  observer.WaitForEvent();
  observer.ClearEvents();

  // Now other_parent has [node2].
  // Add node3 to parent (bookmark_bar_node).
  const bookmarks::BookmarkNode* node3 =
      model_->AddURL(parent, 0, u"Title 3", GURL("http://example3.com"));
  observer.WaitForEvent();
  observer.ClearEvents();

  base::Uuid node2_uuid = GetUuid(node2);
  base::Uuid node3_uuid = GetUuid(node3);

  // Remove all user bookmarks.
  model_->RemoveAllUserBookmarks(FROM_HERE);
  observer.WaitForEvent();

  ASSERT_EQ(observer.events().size(), 2u);
  EXPECT_TRUE(observer.events()[0]->is_removed());
  EXPECT_EQ(observer.events()[0]->get_removed()->id, node3_uuid);
  EXPECT_TRUE(observer.events()[1]->is_removed());
  EXPECT_EQ(observer.events()[1]->get_removed()->id, node2_uuid);
}

TEST_F(BookmarksServiceImplTest, Observation_ExtensiveChanges) {
  mojom::BookmarksSnapshotPtr snapshot;
  {
    base::RunLoop run_loop;
    remote_service_->GetBookmarks(base::BindOnce(
        [](mojom::BookmarksSnapshotPtr* out_snapshot,
           base::OnceClosure quit_closure,
           mojom::BookmarksService::GetBookmarksResult result) {
          ASSERT_TRUE(result.has_value());
          *out_snapshot = std::move(result.value());
          std::move(quit_closure).Run();
        },
        &snapshot, run_loop.QuitClosure()));
    run_loop.Run();
  }

  ASSERT_TRUE(snapshot);
  TestBookmarksObserver observer(std::move(snapshot->stream));

  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  // Start extensive changes (e.g. import).
  model_->BeginExtensiveChanges();

  // Perform multiple changes.
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title 1", GURL("http://example1.com"));
  model_->AddURL(parent, 1, u"Title 2", GURL("http://example2.com"));
  base::Uuid node1_uuid = GetUuid(node1);
  model_->Remove(node1, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);

  // End extensive changes. This should flush the queued events.
  model_->EndExtensiveChanges();

  // Wait for the events to arrive.
  observer.WaitForEvent();

  // We expect 3 events: Add(node1), Add(node2), Remove(node1).
  ASSERT_EQ(observer.events().size(), 3u);
  EXPECT_EQ(observer.call_count(), 1);

  EXPECT_TRUE(observer.events()[0]->is_added());
  EXPECT_EQ(observer.events()[0]->get_added()->node->get_url()->title,
            "Title 1");

  EXPECT_TRUE(observer.events()[1]->is_added());
  EXPECT_EQ(observer.events()[1]->get_added()->node->get_url()->title,
            "Title 2");

  EXPECT_TRUE(observer.events()[2]->is_removed());
  EXPECT_EQ(observer.events()[2]->get_removed()->id, node1_uuid);
}

}  // namespace
}  // namespace bookmarks_api
