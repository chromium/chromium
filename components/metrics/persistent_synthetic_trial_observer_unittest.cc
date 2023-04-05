// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persistent_synthetic_trial_observer.h"

#include "base/test/task_environment.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trials.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class PersistentSyntheticTrialObserverTest : public testing::Test {
 public:
  constexpr static int32_t kAllocatorMemorySize = 1 << 20;  // 1 MiB

  PersistentSyntheticTrialObserverTest() = default;
  ~PersistentSyntheticTrialObserverTest() override = default;

  void SetUp() override {
    Test::SetUp();

    memory_allocator_ = std::make_unique<base::LocalPersistentMemoryAllocator>(
        kAllocatorMemorySize, 0, "");
    GlobalPersistentSystemProfile::GetInstance()->RegisterPersistentAllocator(
        memory_allocator_.get());
    SystemProfileProto profile;
    profile.set_client_uuid("id");
    GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(profile,
                                                                   false);
  }

  void TearDown() override {
    GlobalPersistentSystemProfile::GetInstance()->DeregisterPersistentAllocator(
        memory_allocator_.get());
    Test::TearDown();
  }

  SystemProfileProto GetSystemProfile() {
    SystemProfileProto profile;
    GlobalPersistentSystemProfile::GetInstance()->GetSystemProfile(
        *memory_allocator_, &profile);
    return profile;
  }

 protected:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<base::PersistentMemoryAllocator> memory_allocator_;
};

TEST_F(PersistentSyntheticTrialObserverTest, AddRemoveTrials) {
  PersistentSyntheticTrialObserver observer;
  const variations::SyntheticTrialGroup kGroup1(
      "Trial1", "Group1",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
  const variations::SyntheticTrialGroup kGroup2(
      "Trial2", "Group2", variations::SyntheticTrialAnnotationMode::kNextLog);
  const variations::SyntheticTrialGroup kGroup3(
      "Trial2", "Group3",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

  observer.OnSyntheticTrialsChanged({kGroup1, kGroup2}, {}, {kGroup1, kGroup2});
  SystemProfileProto profile = GetSystemProfile();
  ASSERT_EQ(1, profile.field_trial_size());
  EXPECT_EQ(variations::HashName("Trial1"), profile.field_trial(0).name_id());
  EXPECT_EQ(variations::HashName("Group1"), profile.field_trial(0).group_id());

  observer.OnSyntheticTrialsChanged({kGroup3}, {kGroup1}, {kGroup3});
  profile = GetSystemProfile();
  ASSERT_EQ(1, profile.field_trial_size());
  EXPECT_EQ(variations::HashName("Trial2"), profile.field_trial(0).name_id());
  EXPECT_EQ(variations::HashName("Group3"), profile.field_trial(0).group_id());
}

}  // namespace metrics
