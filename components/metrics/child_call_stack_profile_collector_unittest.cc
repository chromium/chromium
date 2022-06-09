// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/child_call_stack_profile_collector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class ChildCallStackProfileCollectorTest : public testing::Test {
 protected:
  class Receiver : public mojom::CallStackProfileCollector {
   public:
    explicit Receiver(
        mojo::PendingReceiver<mojom::CallStackProfileCollector> receiver)
        : receiver_(this, std::move(receiver)) {}

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    ~Receiver() override {}

    void Collect(base::TimeTicks start_timestamp,
                 mojom::SampledProfilePtr profile) override {
      profile_start_times.push_back(start_timestamp);
    }

    std::vector<base::TimeTicks> profile_start_times;

   private:
    mojo::Receiver<mojom::CallStackProfileCollector> receiver_;
  };

  ChildCallStackProfileCollectorTest()
      : receiver_impl_(
            new Receiver(collector_remote_.InitWithNewPipeAndPassReceiver())) {}

  ChildCallStackProfileCollectorTest(
      const ChildCallStackProfileCollectorTest&) = delete;
  ChildCallStackProfileCollectorTest& operator=(
      const ChildCallStackProfileCollectorTest&) = delete;

  void CollectEmptyProfile(base::TimeTicks start_time) {
    child_collector_.Collect(start_time, SampledProfile());
  }

  const std::vector<ChildCallStackProfileCollector::ProfileState>& profiles()
      const {
    return child_collector_.profiles_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  mojo::PendingRemote<mojom::CallStackProfileCollector> collector_remote_;
  std::unique_ptr<Receiver> receiver_impl_;
  ChildCallStackProfileCollector child_collector_;
};

// Test the behavior when an interface is provided.
TEST_F(ChildCallStackProfileCollectorTest, InterfaceProvided) {
  EXPECT_EQ(0u, profiles().size());

  // Add a profile before providing the interface.
  base::TimeTicks start_timestamp = task_environment_.NowTicks();
  CollectEmptyProfile(start_timestamp);
  ASSERT_EQ(1u, profiles().size());
  EXPECT_EQ(start_timestamp, profiles()[0].start_timestamp);

  // Set the interface. The profiles should be passed to it.
  child_collector_.SetParentProfileCollector(std::move(collector_remote_));
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(0u, profiles().size());
  ASSERT_EQ(1u, receiver_impl_->profile_start_times.size());
  EXPECT_EQ(start_timestamp, receiver_impl_->profile_start_times[0]);

  // Add a profile after providing the interface. It should also be passed.
  receiver_impl_->profile_start_times.clear();
  CollectEmptyProfile(start_timestamp);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(0u, profiles().size());
  ASSERT_EQ(1u, receiver_impl_->profile_start_times.size());
  EXPECT_EQ(start_timestamp, receiver_impl_->profile_start_times[0]);
}

TEST_F(ChildCallStackProfileCollectorTest, InterfaceNotProvided) {
  EXPECT_EQ(0u, profiles().size());

  // Add a profile before providing a null interface.
  base::TimeTicks start_timestamp = task_environment_.NowTicks();
  CollectEmptyProfile(start_timestamp);
  ASSERT_EQ(1u, profiles().size());
  EXPECT_EQ(start_timestamp, profiles()[0].start_timestamp);

  // Set the null interface. The profile should be flushed.
  child_collector_.SetParentProfileCollector(mojo::NullRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, profiles().size());

  // Add a profile after providing a null interface. They should also be
  // flushed.
  CollectEmptyProfile(start_timestamp);
  EXPECT_EQ(0u, profiles().size());
}

}  // namespace metrics
