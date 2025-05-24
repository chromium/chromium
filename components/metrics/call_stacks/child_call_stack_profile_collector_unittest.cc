// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/child_call_stack_profile_collector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class ChildCallStackProfileCollectorTest : public ::testing::Test {
 protected:
  struct ReceivedProfile {
    base::TimeTicks start_timestamp;
    mojom::ProfileType profile_type;
  };

  class Receiver : public mojom::CallStackProfileCollector {
   public:
    explicit Receiver(
        mojo::PendingReceiver<mojom::CallStackProfileCollector> receiver)
        : receiver_(this, std::move(receiver)) {}

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    ~Receiver() override = default;

    void Collect(base::TimeTicks start_timestamp,
                 mojom::ProfileType profile_type,
                 mojom::SampledProfilePtr profile) override {
      profiles_.push_back({start_timestamp, profile_type});
    }

    std::vector<ReceivedProfile>& profiles() { return profiles_; }

   private:
    mojo::Receiver<mojom::CallStackProfileCollector> receiver_;
    std::vector<ReceivedProfile> profiles_;
  };

  ChildCallStackProfileCollectorTest()
      : receiver_impl_(std::make_unique<Receiver>(
            collector_remote_.InitWithNewPipeAndPassReceiver())) {}

  ChildCallStackProfileCollectorTest(
      const ChildCallStackProfileCollectorTest&) = delete;
  ChildCallStackProfileCollectorTest& operator=(
      const ChildCallStackProfileCollectorTest&) = delete;

  // Collects a profile and returns its start timestamp.
  base::TimeTicks CollectProfile(SampledProfile::TriggerEvent trigger_event) {
    base::TimeTicks start_timestamp = task_environment_.NowTicks();
    SampledProfile profile;
    profile.set_trigger_event(trigger_event);
    child_collector_.Collect(start_timestamp, std::move(profile));
    return start_timestamp;
  }

  void ExpectProfile(const ReceivedProfile& profile,
                     base::TimeTicks expected_start_timestamp,
                     mojom::ProfileType expected_profile_type) {
    EXPECT_EQ(expected_start_timestamp, profile.start_timestamp);
    EXPECT_EQ(expected_profile_type, profile.profile_type);
  }

  void ExpectProfile(
      const ChildCallStackProfileCollector::ProfileState& profile,
      base::TimeTicks expected_start_timestamp,
      mojom::ProfileType expected_profile_type) {
    EXPECT_EQ(expected_start_timestamp, profile.start_timestamp);
    EXPECT_EQ(expected_profile_type, profile.profile_type);
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
  base::TimeTicks start_timestamp =
      CollectProfile(SampledProfile::PERIODIC_COLLECTION);
  ASSERT_EQ(1u, profiles().size());
  ExpectProfile(profiles()[0], start_timestamp, mojom::ProfileType::kCPU);

  // Set the interface. The profiles should be passed to it.
  child_collector_.SetParentProfileCollector(std::move(collector_remote_));
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(0u, profiles().size());
  ASSERT_EQ(1u, receiver_impl_->profiles().size());
  ExpectProfile(receiver_impl_->profiles()[0], start_timestamp,
                mojom::ProfileType::kCPU);

  // Add a profile after providing the interface. It should also be passed.
  receiver_impl_->profiles().clear();
  base::TimeTicks start_timestamp2 =
      CollectProfile(SampledProfile::PERIODIC_COLLECTION);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(0u, profiles().size());
  ASSERT_EQ(1u, receiver_impl_->profiles().size());
  ExpectProfile(receiver_impl_->profiles()[0], start_timestamp2,
                mojom::ProfileType::kCPU);
}

TEST_F(ChildCallStackProfileCollectorTest, InterfaceNotProvided) {
  EXPECT_EQ(0u, profiles().size());

  // Add a profile before providing a null interface.
  base::TimeTicks start_timestamp =
      CollectProfile(SampledProfile::PERIODIC_COLLECTION);
  ASSERT_EQ(1u, profiles().size());
  ExpectProfile(profiles()[0], start_timestamp, mojom::ProfileType::kCPU);

  // Set the null interface. The profile should be flushed.
  child_collector_.SetParentProfileCollector(mojo::NullRemote());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(0u, profiles().size());
  EXPECT_EQ(0u, receiver_impl_->profiles().size());

  // Add a profile after providing a null interface. They should also be
  // flushed.
  CollectProfile(SampledProfile::PERIODIC_COLLECTION);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(0u, profiles().size());
  EXPECT_EQ(0u, receiver_impl_->profiles().size());
}

// Tests that the `profile_type` parameter is set correctly when heap profiles
// pass through the interface.
TEST_F(ChildCallStackProfileCollectorTest, HeapProfiles) {
  EXPECT_EQ(0u, profiles().size());

  // Add a profile before providing the interface.
  base::TimeTicks start_timestamp =
      CollectProfile(SampledProfile::PERIODIC_HEAP_COLLECTION);
  ASSERT_EQ(1u, profiles().size());
  ExpectProfile(profiles()[0], start_timestamp, mojom::ProfileType::kHeap);

  // Set the interface. The profile type should pass to it unchanged.
  child_collector_.SetParentProfileCollector(std::move(collector_remote_));
  task_environment_.FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(1u, receiver_impl_->profiles().size());
  ExpectProfile(receiver_impl_->profiles()[0], start_timestamp,
                mojom::ProfileType::kHeap);
}

}  // namespace metrics
