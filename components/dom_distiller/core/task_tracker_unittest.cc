// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/task_tracker.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_content_store.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace dom_distiller {
namespace test {

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  ~FakeViewRequestDelegate() override = default;
  MOCK_METHOD1(OnArticleReady,
               void(const DistilledArticleProto* article_proto));
  MOCK_METHOD1(OnArticleUpdated,
               void(ArticleDistillationUpdate article_update));
};

class MockContentStore : public DistilledContentStore {
 public:
  void LoadContent(const ArticleEntry& entry, LoadCallback callback) override {
    LoadContent_(entry, callback);
  }
  MOCK_METHOD2(LoadContent_,
               void(const ArticleEntry& entry, LoadCallback& callback));
  MOCK_METHOD3(SaveContent,
               void(const ArticleEntry& entry,
                    const DistilledArticleProto& proto,
                    SaveCallback callback));
};

class TestCancelCallback {
 public:
  TestCancelCallback() : cancelled_(false) {}
  TaskTracker::CancelCallback GetCallback() {
    return base::BindOnce(&TestCancelCallback::Cancel, base::Unretained(this));
  }
  void Cancel(TaskTracker*) { cancelled_ = true; }
  bool Cancelled() { return cancelled_; }

 private:
  bool cancelled_;
};

class MockSaveCallback {
 public:
  MOCK_METHOD3(Save,
               void(const ArticleEntry&, const DistilledArticleProto*, bool));
};

class DomDistillerTaskTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    entry_id_ = "id0";
    page_0_url_ = GURL("http://www.example.com/1");
    page_1_url_ = GURL("http://www.example.com/2");
  }

  ArticleEntry GetDefaultEntry() {
    ArticleEntry entry;
    entry.entry_id = entry_id_;
    entry.pages.push_back(page_0_url_);
    entry.pages.push_back(page_1_url_);
    return entry;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string entry_id_;
  GURL page_0_url_;
  GURL page_1_url_;
};

TEST_F(DomDistillerTaskTrackerTest, TestHasEntryId) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);
  EXPECT_TRUE(task_tracker.HasEntryId(entry_id_));
  EXPECT_FALSE(task_tracker.HasEntryId("other_id"));
}

TEST_F(DomDistillerTaskTrackerTest, TestHasUrl) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);
  EXPECT_TRUE(task_tracker.HasUrl(page_0_url_));
  EXPECT_TRUE(task_tracker.HasUrl(page_1_url_));
  EXPECT_FALSE(task_tracker.HasUrl(GURL("http://other.url/")));
}

TEST_F(DomDistillerTaskTrackerTest, TestViewerCancelled) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);

  FakeViewRequestDelegate viewer_delegate;
  FakeViewRequestDelegate viewer_delegate2;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  std::unique_ptr<ViewerHandle> handle2(
      task_tracker.AddViewer(&viewer_delegate2));

  EXPECT_FALSE(cancel_callback.Cancelled());
  handle.reset();
  EXPECT_FALSE(cancel_callback.Cancelled());
  handle2.reset();
  EXPECT_TRUE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestViewerCancelledWithSaveRequest) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  EXPECT_FALSE(cancel_callback.Cancelled());

  MockSaveCallback save_callback;
  task_tracker.AddSaveCallback(base::BindOnce(
      &MockSaveCallback::Save, base::Unretained(&save_callback)));
  handle.reset();

  // Since there is a pending save request, the task shouldn't be cancelled.
  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestViewerNotifiedOnDistillationComplete) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(true);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(viewer_delegate, OnArticleReady(_));

  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestDistillerFails) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(viewer_delegate, OnArticleReady(_));

  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());
  distiller->RunDistillerCallback(std::make_unique<DistilledArticleProto>());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest,
       TestSaveCallbackCalledOnDistillationComplete) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(true);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           nullptr);

  MockSaveCallback save_callback;
  task_tracker.AddSaveCallback(base::BindOnce(
      &MockSaveCallback::Save, base::Unretained(&save_callback)));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(save_callback, Save(_, _, _));

  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(cancel_callback.Cancelled());
}

DistilledArticleProto CreateDistilledArticleForEntry(
    const ArticleEntry& entry) {
  DistilledArticleProto article;
  for (const GURL& url : entry.pages) {
    DistilledPageProto* page = article.add_pages();
    page->set_url(url.spec());
    page->set_html("<div>" + url.spec() + "</div>");
  }
  return article;
}

TEST_F(DomDistillerTaskTrackerTest, TestBlobFetcher) {
  ArticleEntry entry_with_blob = GetDefaultEntry();
  DistilledArticleProto stored_distilled_article =
      CreateDistilledArticleForEntry(entry_with_blob);
  InMemoryContentStore content_store(kDefaultMaxNumCachedEntries);
  content_store.InjectContent(entry_with_blob, stored_distilled_article);
  TestCancelCallback cancel_callback;

  TaskTracker task_tracker(entry_with_blob, cancel_callback.GetCallback(),
                           &content_store);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  base::RunLoop().RunUntilIdle();

  const DistilledArticleProto* distilled_article;

  EXPECT_CALL(viewer_delegate, OnArticleReady(_))
      .WillOnce(testing::SaveArg<0>(&distilled_article));

  task_tracker.StartBlobFetcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(stored_distilled_article.SerializeAsString(),
            distilled_article->SerializeAsString());

  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestBlobFetcherFinishesFirst) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  ArticleEntry entry_with_blob = GetDefaultEntry();
  DistilledArticleProto stored_distilled_article =
      CreateDistilledArticleForEntry(entry_with_blob);
  InMemoryContentStore content_store(kDefaultMaxNumCachedEntries);
  content_store.InjectContent(entry_with_blob, stored_distilled_article);
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(entry_with_blob, cancel_callback.GetCallback(),
                           &content_store);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  base::RunLoop().RunUntilIdle();

  DistilledArticleProto distilled_article;

  EXPECT_CALL(viewer_delegate, OnArticleReady(_))
      .WillOnce(testing::SaveArgPointee<0>(&distilled_article));
  bool distiller_destroyed = false;
  EXPECT_CALL(*distiller, Die())
      .WillOnce(testing::Assign(&distiller_destroyed, true));

  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());
  task_tracker.StartBlobFetcher();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&viewer_delegate);
  EXPECT_EQ(stored_distilled_article.SerializeAsString(),
            distilled_article.SerializeAsString());

  EXPECT_TRUE(distiller_destroyed);
  EXPECT_FALSE(cancel_callback.Cancelled());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DomDistillerTaskTrackerTest, TestBlobFetcherWithoutBlob) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  ArticleEntry entry(GetDefaultEntry());
  InMemoryContentStore content_store(kDefaultMaxNumCachedEntries);
  std::unique_ptr<DistilledArticleProto> distilled_article(
      new DistilledArticleProto(CreateDistilledArticleForEntry(entry)));

  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           &content_store);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));
  base::RunLoop().RunUntilIdle();

  task_tracker.StartBlobFetcher();
  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());

  // OnArticleReady shouldn't be called until distillation finishes (i.e. the
  // blob fetcher shouldn't return distilled content).
  EXPECT_CALL(viewer_delegate, OnArticleReady(_)).Times(0);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(viewer_delegate, OnArticleReady(_));
  distiller->RunDistillerCallback(std::move(distilled_article));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestDistillerFailsFirst) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  ArticleEntry entry(GetDefaultEntry());
  MockContentStore content_store;

  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           &content_store);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));

  DistilledContentStore::LoadCallback content_store_load_callback;
  EXPECT_CALL(content_store, LoadContent_(_, _))
      .WillOnce(MoveArg<1>(&content_store_load_callback));

  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());
  task_tracker.StartBlobFetcher();

  EXPECT_CALL(viewer_delegate, OnArticleReady(_)).Times(0);
  distiller->RunDistillerCallback(std::make_unique<DistilledArticleProto>());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(viewer_delegate, OnArticleReady(_));
  std::move(content_store_load_callback)
      .Run(true, std::make_unique<DistilledArticleProto>(
                     CreateDistilledArticleForEntry(entry)));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, ContentIsSaved) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  ArticleEntry entry(GetDefaultEntry());
  DistilledArticleProto distilled_article =
      CreateDistilledArticleForEntry(entry);

  MockContentStore content_store;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback(),
                           &content_store);

  FakeViewRequestDelegate viewer_delegate;
  std::unique_ptr<ViewerHandle> handle(
      task_tracker.AddViewer(&viewer_delegate));

  DistilledArticleProto stored_distilled_article;
  DistilledContentStore::LoadCallback content_store_load_callback;
  EXPECT_CALL(content_store, SaveContent(_, _, _))
      .WillOnce(testing::SaveArg<1>(&stored_distilled_article));

  task_tracker.StartDistiller(&distiller_factory,
                              std::unique_ptr<DistillerPage>());

  EXPECT_CALL(viewer_delegate, OnArticleReady(_));
  distiller->RunDistillerCallback(
      std::make_unique<DistilledArticleProto>(distilled_article));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(stored_distilled_article.SerializeAsString(),
            distilled_article.SerializeAsString());
  EXPECT_FALSE(cancel_callback.Cancelled());
}

}  // namespace test
}  // namespace dom_distiller
