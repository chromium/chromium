// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/media_resource_tracker.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromecast/media/common/test_media_resource_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class MediaResourceTrackerTest : public ::testing::Test {
 public:
  MediaResourceTrackerTest() {}

  MediaResourceTrackerTest(const MediaResourceTrackerTest&) = delete;
  MediaResourceTrackerTest& operator=(const MediaResourceTrackerTest&) = delete;

  ~MediaResourceTrackerTest() override {}

 protected:
  void SetUp() override {
    test_mocks_.reset(new MediaResourceTrackerTestMocks());

    resource_tracker_ = new TestMediaResourceTracker(
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(), test_mocks_.get());
  }

  void InitializeMediaLib() {
    EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
    resource_tracker_->InitializeMediaLib();
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestMediaResourceTracker* resource_tracker_;
  std::unique_ptr<MediaResourceTrackerTestMocks> test_mocks_;
};

TEST_F(MediaResourceTrackerTest, BasicLifecycle) {
  // Startup and shutdown flow: Initialize then FinalizeAndDestroy
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);

  resource_tracker_->InitializeMediaLib();
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, InitializeTwice) {
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);

  resource_tracker_->InitializeMediaLib();
  resource_tracker_->InitializeMediaLib();
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, FinalizeWithoutInitialize) {
  EXPECT_CALL(*test_mocks_, Initialize()).Times(0);
  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);

  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();
}

// Check FinalizeCastMediaShlib works correctly with no users of
// media resource.
TEST_F(MediaResourceTrackerTest, FinalizeResourceNotInUse) {
  InitializeMediaLib();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  EXPECT_CALL(*test_mocks_, FinalizeCallback()).Times(1);
  resource_tracker_->FinalizeMediaLib(
      base::BindOnce(&MediaResourceTrackerTestMocks::FinalizeCallback,
                     base::Unretained(test_mocks_.get())));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();
}

// Check FinalizeCastMediaShlib waits for resource to not be in use.
TEST_F(MediaResourceTrackerTest, FinalizeResourceInUse) {
  InitializeMediaLib();

  resource_tracker_->IncrementUsageCount();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  resource_tracker_->FinalizeMediaLib(
      base::BindOnce(&MediaResourceTrackerTestMocks::FinalizeCallback,
                     base::Unretained(test_mocks_.get())));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, FinalizeCallback()).Times(1);
  resource_tracker_->DecrementUsageCount();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();
}

// Check FinalizeAndDestroy waits for resource to not be in use.
TEST_F(MediaResourceTrackerTest, DestroyWaitForNoUsers) {
  InitializeMediaLib();

  resource_tracker_->IncrementUsageCount();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  resource_tracker_->DecrementUsageCount();
  base::RunLoop().RunUntilIdle();
}

// Check finalize callback still made if FinalizeAndDestroy called
// while waiting for resource usage to end.
TEST_F(MediaResourceTrackerTest, DestroyWithPendingFinalize) {
  InitializeMediaLib();

  resource_tracker_->IncrementUsageCount();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  resource_tracker_->FinalizeMediaLib(
      base::BindOnce(&MediaResourceTrackerTestMocks::FinalizeCallback,
                     base::Unretained(test_mocks_.get())));
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  EXPECT_CALL(*test_mocks_, FinalizeCallback()).Times(1);
  resource_tracker_->DecrementUsageCount();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, ScopedUsage) {
  InitializeMediaLib();

  EXPECT_EQ(0u, resource_tracker_->media_use_count());
  {
    std::unique_ptr<MediaResourceTracker::ScopedUsage> scoped_usage(
        new MediaResourceTracker::ScopedUsage(resource_tracker_));
    EXPECT_EQ(1u, resource_tracker_->media_use_count());
  }
  EXPECT_EQ(0u, resource_tracker_->media_use_count());

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  resource_tracker_->FinalizeAndDestroy();
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
}  // namespace chromecast
