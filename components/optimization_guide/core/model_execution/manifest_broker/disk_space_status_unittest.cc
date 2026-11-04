// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/disk_space_status.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class FakeManifestMonitorDelegate : public ManifestMonitorDelegate {
 public:
  base::CallbackListSubscription ListenForManifestReady(
      base::RepeatingCallback<void(base::FilePath)> on_ready) override {
    return base::CallbackListSubscription();
  }

  void GetFreeDiskSpace(base::OnceCallback<void(std::optional<base::ByteSize>)>
                            callback) const override {
    pending_callbacks_.push_back(std::move(callback));
  }

  size_t pending_count() const { return pending_callbacks_.size(); }

  void ResolveNext(std::optional<base::ByteSize> free_space) {
    ASSERT_FALSE(pending_callbacks_.empty());
    auto callback = std::move(pending_callbacks_.front());
    pending_callbacks_.erase(pending_callbacks_.begin());
    std::move(callback).Run(free_space);
  }

 private:
  mutable std::vector<base::OnceCallback<void(std::optional<base::ByteSize>)>>
      pending_callbacks_;
};

TEST(DiskSpaceStatusTest, EvaluatesDeduplicatesAndExpiresAfterTtl) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestMonitorDelegate delegate;
  std::vector<std::string> events;

  DiskSpaceStatus status(delegate);

  EXPECT_FALSE(status.IsFresh());
  EXPECT_FALSE(status.IsEvaluationPending());
  EXPECT_FALSE(status.IsSufficientForOnDemandInstall());
  EXPECT_EQ(status.free_space(), std::nullopt);

  status.EnsureEvaluated(
      base::BindLambdaForTesting([&]() { events.push_back("cb1"); }));
  EXPECT_TRUE(status.IsEvaluationPending());
  status.EnsureEvaluated(
      base::BindLambdaForTesting([&]() { events.push_back("cb2"); }));
  // Concurrent EnsureEvaluated() calls are deduplicated into one delegate call.
  EXPECT_EQ(delegate.pending_count(), 1u);
  EXPECT_TRUE(events.empty());

  delegate.ResolveNext(base::GiB(25));
  EXPECT_TRUE(status.IsFresh());
  EXPECT_FALSE(status.IsEvaluationPending());
  EXPECT_TRUE(status.IsSufficientForOnDemandInstall());
  EXPECT_FALSE(status.IsSufficientForBackgroundInstall());
  EXPECT_EQ(status.free_space(), base::GiB(25));
  EXPECT_EQ(events, (std::vector<std::string>{"cb1", "cb2"}));

  // While fresh (< 10s), EnsureEvaluated() runs immediately without querying
  // the delegate.
  events.clear();
  base::test::TestFuture<void> immediate_future;
  status.EnsureEvaluated(immediate_future.GetCallback());
  EXPECT_TRUE(immediate_future.IsReady());
  EXPECT_EQ(delegate.pending_count(), 0u);

  // After the 10s freshness TTL expires, EnsureEvaluated() queries again.
  task_environment.FastForwardBy(base::Seconds(11));
  EXPECT_FALSE(status.IsFresh());
}

}  // namespace
}  // namespace optimization_guide
