// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/test_media_resource_tracker.h"

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

MediaResourceTrackerTestMocks::MediaResourceTrackerTestMocks() = default;

MediaResourceTrackerTestMocks::~MediaResourceTrackerTestMocks() = default;

TestMediaResourceTracker::TestMediaResourceTracker(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    MediaResourceTrackerTestMocks* test_mocks)
    : MediaResourceTracker(std::move(ui_task_runner),
                           std::move(media_task_runner)),
      test_mocks_(test_mocks) {}

TestMediaResourceTracker::~TestMediaResourceTracker() {
  EXPECT_TRUE(ui_task_runner_->BelongsToCurrentThread());
  if (test_mocks_)
    test_mocks_->Destroyed();
}

void TestMediaResourceTracker::DoInitializeMediaLib() {
  ASSERT_TRUE(media_task_runner_->BelongsToCurrentThread());
  if (test_mocks_)
    test_mocks_->Initialize();
}

void TestMediaResourceTracker::DoFinalizeMediaLib() {
  ASSERT_TRUE(media_task_runner_->BelongsToCurrentThread());
  if (test_mocks_)
    test_mocks_->Finalize();
}

}  // namespace media
}  // namespace chromecast
