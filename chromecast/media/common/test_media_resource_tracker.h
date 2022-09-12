// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains classes to aid in testing code that uses MediaResourceTracker.

#ifndef CHROMECAST_MEDIA_COMMON_TEST_MEDIA_RESOURCE_TRACKER_H_
#define CHROMECAST_MEDIA_COMMON_TEST_MEDIA_RESOURCE_TRACKER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/common/media_resource_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

// Collection of mocks to verify MediaResourceTracker takes the correct actions.
class MediaResourceTrackerTestMocks {
 public:
  MediaResourceTrackerTestMocks();
  ~MediaResourceTrackerTestMocks();
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(Finalize, void());
  MOCK_METHOD0(Destroyed, void());
  MOCK_METHOD0(FinalizeCallback, void());
};

class TestMediaResourceTracker : public MediaResourceTracker {
 public:
  TestMediaResourceTracker(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      MediaResourceTrackerTestMocks* test_mocks);

  TestMediaResourceTracker(const TestMediaResourceTracker&) = delete;
  TestMediaResourceTracker& operator=(const TestMediaResourceTracker&) = delete;

  ~TestMediaResourceTracker() override;

  size_t media_use_count() const { return media_use_count_; }

 private:
  // MediaResourceTracker implementation:
  void DoInitializeMediaLib() override;
  void DoFinalizeMediaLib() override;

  MediaResourceTrackerTestMocks* const test_mocks_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_COMMON_TEST_MEDIA_RESOURCE_TRACKER_H_
