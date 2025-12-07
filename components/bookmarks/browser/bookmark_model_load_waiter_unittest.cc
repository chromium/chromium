// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model_load_waiter.h"

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

namespace {

class BookmarkModelLoadWaiterTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<BookmarkModel> model_{
      std::make_unique<BookmarkModel>(std::make_unique<TestBookmarkClient>())};
};

TEST_F(BookmarkModelLoadWaiterTest, ShouldRunCallbackWhenModelAlreadyLoaded) {
  model_->LoadEmptyForTest();
  ASSERT_TRUE(model_->loaded());

  base::RunLoop run_loop;
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  ScheduleCallbackOnBookmarkModelLoad(*model_, run_loop.QuitClosure());

  run_loop.Run();
}

TEST_F(BookmarkModelLoadWaiterTest, ShouldRunCallbackUponModelLoad) {
  ASSERT_FALSE(model_->loaded());

  base::RunLoop run_loop;
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  ScheduleCallbackOnBookmarkModelLoad(*model_, run_loop.QuitClosure());

  model_->LoadEmptyForTest();
  run_loop.Run();
}

TEST_F(BookmarkModelLoadWaiterTest,
       ShouldSafelyRunCallbackWhenCallbackDestroysBookmarkModel) {
  model_->LoadEmptyForTest();
  ASSERT_TRUE(model_->loaded());

  base::RunLoop run_loop;
  base::MockOnceClosure mock_callback;
  ON_CALL(mock_callback, Run).WillByDefault([&run_loop, this] {
    model_.reset();
    run_loop.Quit();
  });

  EXPECT_CALL(mock_callback, Run).Times(0);

  ScheduleCallbackOnBookmarkModelLoad(*model_, mock_callback.Get());

  EXPECT_CALL(mock_callback, Run);
  run_loop.Run();

  ASSERT_FALSE(model_);
}

TEST_F(BookmarkModelLoadWaiterTest,
       ShouldNotRunCallbackIfModelDestroyedBeforeLoadingCompletes) {
  ASSERT_FALSE(model_->loaded());

  base::MockOnceClosure mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  ScheduleCallbackOnBookmarkModelLoad(*model_, mock_callback.Get());

  // Destroy the model before it's loaded.
  model_.reset();
}

TEST_F(BookmarkModelLoadWaiterTest,
       ShouldNotRunCallbackIfModelAlreadyLoadedButDestroyed) {
  model_->LoadEmptyForTest();
  ASSERT_TRUE(model_->loaded());

  base::MockOnceClosure mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  ScheduleCallbackOnBookmarkModelLoad(*model_, mock_callback.Get());

  // Destroy the loaded model before callback is run.
  model_.reset();

  // This makes sure the posted task attempts to run the callback.
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace bookmarks
